# jpeg_save 模块 —— 代码审查 & jpeg_save_frame 实现指南

> 日期：2026-06-10
> 状态：h 文件基本 OK，c 文件 init/destroy 有 bug，frame 函数待写

---

## 一、jpeg_save.h 审查

**已修复的问题：**
- ✅ `#include <stddef.h>` 已加上（size_t）
- ✅ `int width, height;` 逗号后空格已修正

**还需修改：**
- ⬜ 需要加 `#include <jpeglib.h>` 和 `#include <setjmp.h>`
- ⬜ 结构体需要加两个内部字段（见下方）

```c
// jpeg_save.h 需要新增的内容

#include <jpeglib.h>
#include <setjmp.h>

// 自定义错误管理器（libjpeg 要求第一个成员必须是 jpeg_error_mgr）
struct my_error_mgr {
    struct jpeg_error_mgr pub;   // 必须第一个成员
    jmp_buf setjmp_buffer;       // setjmp/longjmp 跳转点
};

typedef struct jpeg_save_t {
    char output_dir[256];
    int quality;
    int width, height;

    /* ↓ 新增：libjpeg 内部用 */
    struct jpeg_compress_struct cinfo;   // libjpeg 压缩对象
    struct my_error_mgr jerr;            // 自定义错误管理器
} jpeg_save_t;
```

---

## 二、jpeg_save.c 审查

### jpeg_save_init — 3 个 bug

| # | 行号 | 问题 | 严重度 | 修复方法 |
|---|------|------|--------|----------|
| 1 | 12 | `w == NULL \|\| h == NULL \|\| quality == NULL` — int 不能和 NULL 比较 | 🔴 类型错误 | 改为 `w <= 0 \|\| h <= 0 \|\| quality < 1 \|\| quality > 100` |
| 2 | 16 | `js->output_dir = dir` — 数组不能被赋值 | 🔴 编译失败 | 改为 `strncpy(js->output_dir, dir, sizeof(js->output_dir) - 1)` |
| 3 | 20 | 函数声明返回 int，成功路径缺少 `return 0` | 🟡 逻辑 bug | 末尾加 `return 0;` |
| 4 | 9,13 | `fprintf` 缺 `\n`，日志可能卡缓冲区 | 🟢 风格 | 两处都加上 `\n` |

**此外 jpeg_save_init 还需新增：**
- `js->cinfo.err = jpeg_std_error(&js->jerr.pub)` — 设置错误管理器
- `js->jerr.pub.error_exit = 自定义错误回调` — 覆盖默认 exit(1)
- `jpeg_create_compress(&js->cinfo)` — 创建压缩对象
- 任何步骤失败要 `jpeg_destroy_compress(&js->cinfo)` 回滚

### jpeg_save_destroy — 2 个严重 bug

| # | 行号 | 问题 | 严重度 | 修复方法 |
|---|------|------|--------|----------|
| 1 | 29 | `free(js->output_dir)` — output_dir 是嵌入式 `char[256]` 数组，不是 malloc 出来的。free 非堆地址 → 段错误 | 🔴 运行时崩溃 | 删除这行 |
| 2 | 30 | `free(js)` — jpeg_save_init 是调用者提供内存模式（和 gpio_alert_init 一样），destroy 不应 free 结构体本身 | 🔴 运行时崩溃 | 删除这行，改为 `jpeg_destroy_compress(&js->cinfo)` |
| 3 | 26 | `fprintf` 缺 `\n` | 🟢 风格 | 加上 `\n` |

**正确模式对比：**

| 模块 | init 签名 | 谁分配内存 | destroy 是否 free 结构体 |
|------|-----------|-----------|------------------------|
| gpio_alert | `int gpio_alert_init(gpio_alert_t *gpio, ...)` | 调用者 | ❌ 不 free |
| motion_detect | `int motion_detect_init(motion_detect_t *md, ...)` | 调用者 | ❌ 不 free |
| **jpeg_save** | `int jpeg_save_init(jpeg_save_t *js, ...)` | **调用者** | ❌ **不 free** |
| sms_alert | `sms_alert_t* sms_alert_init(...)` | 内部 malloc | ✅ 会 free |

---

## 三、YUYV 数据从哪来？

```
/dev/video1
    ↓ VIDIOC_DQBUF                    ← capture_thread.c:38
V4L2 mmap 缓冲区 (YUYV 原始)
    ↓ memcpy(frame->data, ...)        ← capture_thread.c:55
frame_t->data (堆上副本, YUYV 格式)
    ↓ ring_buffer_put()               ← capture_thread.c:66
╔══════════════════╗
║   ring_buffer    ║
╚══════════════════╝
    ↓ ring_buffer_get() / get_nonblock()
frame_t *frame
    ↓ motion_detect_process(md, frame)  ← 先用 Y 分量做三帧差法
    ↓ (return 1 = 检测到运动!)
    ↓ jpeg_save_frame(js, frame->data, ...)  ← frame->data 就是 YUYV 裸数据
```

**关键**：`frame->data` — 640×480×2 = 614,400 字节的 YUYV 裸数据，每 4 字节 = 2 个像素（Y₀-U₀-Y₁-V₀）。

---

## 四、jpeg_save_frame 实现指南

### 4.1 核心难点：YUYV 不是 libjpeg 的原生输入格式

libjpeg 不直接吃 YUYV 交错数据。需要**先把每行 YUYV 转成 RGB，再逐行喂给 libjpeg**。

### 4.2 函数执行流程（7 步）

```
① 参数校验        js / yuyv_data / out_path 是否为 NULL，path_len 是否够
② 生成文件名       time() + localtime() + strftime → "snap_20260610_143021.jpg"
③ 打开文件         fopen(out_path, "wb")
④ 配置压缩参数     cinfo.image_width/height, in_color_space=JCS_RGB,
                   input_components=3, jpeg_stdio_dest, jpeg_set_quality
⑤ 逐行写入         每行循环：YUYV行 → RGB行转换 → jpeg_write_scanlines
⑥ 完成压缩         jpeg_finish_compress
⑦ 清理并返回       jpeg_destroy_compress, fclose, return 0
```

### 4.3 setjmp/longjmp 错误处理（⭐ 关键）

libjpeg 默认出错直接 `exit(1)` — 嵌入式设备上就是进程死。必须用 `setjmp` 设跳回点：

```c
// 错误回调函数（static，只在 .c 内部用）
static void my_error_exit(j_common_ptr cinfo) {
    struct my_error_mgr *myerr = (struct my_error_mgr *)cinfo->err;
    longjmp(myerr->setjmp_buffer, 1);  // 跳回 setjmp，第二个参数=1 表示"出错"
}

// 在 jpeg_save_frame 里使用：
if (setjmp(js->jerr.setjmp_buffer)) {
    // 任何 libjpeg 错误都会 longjmp 跳到这里
    jpeg_destroy_compress(&js->cinfo);
    fclose(fp);
    remove(out_path);  // 删除半成品文件
    return -1;
}
// setjmp 返回 0 = 正常路径，继续执行压缩...
```

这就是为什么头文件里需要 `jmp_buf` —— 它保存了 CPU 寄存器状态（栈指针、指令指针等），`longjmp` 时恢复。

### 4.4 YUYV → RGB 行转换（核心计算）

YUYV 内存布局（每行 640 像素 = 1280 字节）：

```
字节:  0    1    2    3    4    5    6    7   ...
      Y₀   U₀   Y₁   V₀   Y₂   U₁   Y₃   V₁  ...
      └── 像素0 ──┘   └── 像素1 ──┘
```

**每 4 字节 = 2 个像素，U/V 共享。** 转换公式（ITU-R BT.601，整数运算）：

```
对每组 (Y₀, U, Y₁, V)：

  C0 = Y₀ - 16
  C1 = Y₁ - 16
  D  = U  - 128
  E  = V  - 128

  R₀ = clamp((298*C0 + 409*E + 128) >> 8)
  G₀ = clamp((298*C0 - 100*D - 208*E + 128) >> 8)
  B₀ = clamp((298*C0 + 516*D + 128) >> 8)

  R₁ = clamp((298*C1 + 409*E + 128) >> 8)
  G₁ = clamp((298*C1 - 100*D - 208*E + 128) >> 8)
  B₁ = clamp((298*C1 + 516*D + 128) >> 8)

输出 RGB 行: [R₀,G₀,B₀, R₁,G₁,B₁, ...] 共 width×3 字节
```

`clamp(v)` = `v < 0 ? 0 : v > 255 ? 255 : v`

**定点运算解释**：`>> 8` 等价于 `/256`，`+128` 是四舍五入。系数 298/409/100/208/516 是 BT.601 浮点系数 × 256 后的整数值。

### 4.5 libjpeg API 调用序列

```c
// ④ 设置压缩参数
js->cinfo.image_width      = js->width;     // 640
js->cinfo.image_height     = js->height;    // 480
js->cinfo.input_components = 3;             // RGB = 3 通道
js->cinfo.in_color_space   = JCS_RGB;       // 输入色彩空间
jpeg_set_defaults(&js->cinfo);               // 填充默认参数
jpeg_set_quality(&js->cinfo, js->quality, TRUE);  // 质量 1-100
jpeg_stdio_dest(&js->cinfo, fp);             // 输出到文件
jpeg_start_compress(&js->cinfo, TRUE);       // 开始压缩

// ⑤ 逐行写入
unsigned char *rgb_row = malloc(js->width * 3);  // 一行 RGB 缓冲区
JSAMPROW row_pointer[1];                          // libjpeg 行指针数组
for (int row = 0; row < js->height; row++) {
    // yuyv 每行 = width*2 字节，从 yuyv_data + row*width*2 开始
    // 转换 YUYV → RGB 填到 rgb_row
    yuyv_to_rgb_row(yuyv_data + row * js->width * 2, rgb_row, js->width);
    row_pointer[0] = rgb_row;
    jpeg_write_scanlines(&js->cinfo, row_pointer, 1);
}

// ⑥ 完成
jpeg_finish_compress(&js->cinfo);
```

### 4.6 需要的局部变量

| 变量 | 类型 | 用途 |
|------|------|------|
| `fp` | `FILE*` | 输出文件句柄 |
| `rgb_row` | `unsigned char*` | malloc(width*3)，一行 RGB |
| `timestamp` | `time_t` | time() 的返回值 |
| `tm_info` | `struct tm*` | localtime() 的返回值 |
| `row_pointer` | `JSAMPROW[1]` | libjpeg 行指针数组 |

### 4.7 错误路径清理清单

| 失败点 | 需要清理的 |
|--------|-----------|
| `fopen` 失败 | 直接 return -1 |
| `malloc(rgb_row)` 失败 | fclose(fp); remove(out_path); return -1 |
| libjpeg setjmp 跳回（编码错误） | jpeg_destroy_compress; fclose; remove(out_path); free(rgb_row); return -1 |
| 正常完成 | jpeg_finish_compress; jpeg_destroy_compress; fclose; free(rgb_row); return 0 |

---

## 五、修改清单汇总

### 需修改 jpeg_save.h
- [ ] 加 `#include <jpeglib.h>` 和 `#include <setjmp.h>`
- [ ] 加 `struct my_error_mgr` 定义
- [ ] `jpeg_save_t` 加 `struct jpeg_compress_struct cinfo` 和 `struct my_error_mgr jerr`

### 需修改 jpeg_save.c — init
- [ ] 第12行：`== NULL` → 数值范围检查
- [ ] 第16行：`= dir` → `strncpy`
- [ ] 第9,13行：fprintf 加 `\n`
- [ ] 末尾加 `return 0;`
- [ ] 新增：jpeg_std_error + error_exit + jpeg_create_compress
- [ ] 新增：错误路径 jpeg_destroy_compress 回滚

### 需修改 jpeg_save.c — destroy
- [ ] 删 `free(js->output_dir)` — 换 `jpeg_destroy_compress(&js->cinfo)`
- [ ] 删 `free(js)` — 调用者管理结构体内存
- [ ] fprintf 加 `\n`

### 需实现 jpeg_save.c — frame
- [ ] 按 4.2 七步流程实现
- [ ] 写 `my_error_exit` 静态回调函数
- [ ] 写 `yuyv_to_rgb_row` 静态转换函数
- [ ] setjmp 跳回点 + 清理逻辑

---

## 六、YUYV → RGB 转换辅助函数签名

```c
// 内部函数，static，放在 jpeg_save.c
// yuyv_row: 输入，长度 width*2 字节（YUYV 一行）
// rgb_row:  输出，长度 width*3 字节（RGB 一行）
// width:    图像宽度（像素数）
static void yuyv_to_rgb_row(const unsigned char *yuyv_row,
                            unsigned char *rgb_row,
                            int width);
```

每轮循环处理 4 字节输入 → 6 字节输出，共 `width / 2` 轮（width 假设为偶数，640 符合）。

---

## 七、下一步

1. 按此指南修改 jpeg_save.h 和 jpeg_save.c
2. 交叉编译 libjpeg-turbo（Ubuntu VM，和 paho 流程一样）
3. 写 test_jpeg_save.c 自测
4. 集成到 main.c 多线程
