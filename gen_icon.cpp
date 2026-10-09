// 生成程序图标 - 电脑 + 改名标识
// 编译: cl /O2 /EHsc /utf-8 gen_icon.cpp /link gdi32.lib user32.lib
#include <windows.h>
#include <cstdio>
#include <cstring>

#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")

// ICO 文件头
#pragma pack(push, 1)
struct IcoHeader {
    WORD reserved;  // 0
    WORD type;      // 1 = icon
    WORD count;     // 图像数量
};

struct IcoDirEntry {
    BYTE width;     // 0 = 256
    BYTE height;    // 0 = 256
    BYTE colors;    // 0 = no palette
    BYTE reserved;  // 0
    WORD planes;    // 1
    WORD bpp;       // 32
    DWORD size;     // 数据大小
    DWORD offset;   // 数据偏移
};

struct BmpHeader {
    DWORD headerSize;  // 40
    LONG width;
    LONG height;       // 2x for icon
    WORD planes;       // 1
    WORD bpp;          // 32
    DWORD compression; // 0
    DWORD imageSize;
    LONG xPPM;
    LONG yPPM;
    DWORD colorsUsed;
    DWORD colorsImportant;
};
#pragma pack(pop)

// 生成 32x32 图标
// 设计：深色圆角矩形背景 + 白色电脑图标 + 蓝色改名箭头
void GenerateIcon(const char* path) {
    const int W = 32, H = 32;
    DWORD* pixels = new DWORD[W * H];

    // 颜色
    DWORD cBg     = 0xFF252321; // 深灰 BGRA
    DWORD cBlue   = 0xFFD77800; // 蓝色
    DWORD cWhite  = 0xFFFFFFFF; // 白
    DWORD cGreen  = 0xFF50C800; // 绿
    DWORD cTrans  = 0x00000000; // 透明

    // 初始化为透明
    for (int i = 0; i < W * H; i++) pixels[i] = cTrans;

    // 画圆角矩形背景 (28x28 居中)
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            // 圆角判断
            bool inCorner = false;
            int r = 4;
            // 四个圆角中心
            int cx[4] = {2+r, W-1-r, 2+r, W-1-r};
            int cy[4] = {2+r, 2+r, H-1-r, H-1-r};
            if (x < 2+r && y < 2+r) { // 左上
                int dx = x - (2+r), dy = y - (2+r);
                if (dx*dx + dy*dy > r*r) inCorner = true;
            } else if (x >= W-1-r && y < 2+r) { // 右上
                int dx = x - (W-1-r), dy = y - (2+r);
                if (dx*dx + dy*dy > r*r) inCorner = true;
            } else if (x < 2+r && y >= H-1-r) { // 左下
                int dx = x - (2+r), dy = y - (H-1-r);
                if (dx*dx + dy*dy > r*r) inCorner = true;
            } else if (x >= W-1-r && y >= H-1-r) { // 右下
                int dx = x - (W-1-r), dy = y - (H-1-r);
                if (dx*dx + dy*dy > r*r) inCorner = true;
            }
            if (x >= 2 && x < W-2 && y >= 2 && y < H-2 && !inCorner) {
                pixels[y * W + x] = cBg;
            }
        }
    }

    // 画电脑显示器 (白色, 10x7, 居中偏上)
    // 显示器位置: x=11~20, y=8~14
    for (int y = 8; y <= 14; y++) {
        for (int x = 11; x <= 20; x++) {
            if (y == 8 || y == 14 || x == 11 || x == 20) {
                pixels[y * W + x] = cWhite; // 边框
            } else if (y >= 9 && y <= 13 && x >= 12 && x <= 19) {
                pixels[y * W + x] = cBlue; // 屏幕内容蓝色
            }
        }
    }
    // 底座
    for (int x = 13; x <= 18; x++) pixels[15 * W + x] = cWhite;
    pixels[16 * W + 15] = cWhite;
    pixels[16 * W + 16] = cWhite;

    // 画改名箭头 (绿色, 右下角)
    // 一个小编辑标识: 铅笔形状
    // 铅笔: 从 (20,18) 到 (27,25)
    for (int i = 0; i < 7; i++) {
        int x = 20 + i;
        int y = 18 + i;
        if (x < W-2 && y < H-2) {
            pixels[y * W + x] = cGreen;
            if (x+1 < W-2) pixels[y * W + x + 1] = cGreen;
        }
    }
    // 箭头尖
    pixels[25 * W + 26] = cGreen;
    pixels[26 * W + 25] = cGreen;

    // 写 ICO 文件
    FILE* fp = fopen(path, "wb");
    if (!fp) { printf("无法创建文件\n"); return; }

    IcoHeader hdr = {0, 1, 1};
    fwrite(&hdr, sizeof(hdr), 1, fp);

    // BMP 数据
    BmpHeader bmp = {0};
    bmp.headerSize = 40;
    bmp.width = W;
    bmp.height = H * 2; // icon 用 2x 高度
    bmp.planes = 1;
    bmp.bpp = 32;
    bmp.compression = 0;
    bmp.imageSize = W * H * 4;
    bmp.xPPM = 0;
    bmp.yPPM = 0;
    bmp.colorsUsed = 0;
    bmp.colorsImportant = 0;

    DWORD dataSize = sizeof(BmpHeader) + W * H * 4;
    IcoDirEntry entry = {0};
    entry.width = W;
    entry.height = H;
    entry.colors = 0;
    entry.reserved = 0;
    entry.planes = 1;
    entry.bpp = 32;
    entry.size = dataSize;
    entry.offset = sizeof(IcoHeader) + sizeof(IcoDirEntry);
    fwrite(&entry, sizeof(entry), 1, fp);

    // 写 BMP 头
    fwrite(&bmp, sizeof(bmp), 1, fp);

    // 写像素 (从下到上, BGRA)
    for (int y = H - 1; y >= 0; y--) {
        for (int x = 0; x < W; x++) {
            DWORD p = pixels[y * W + x];
            BYTE b = p & 0xFF;
            BYTE g = (p >> 8) & 0xFF;
            BYTE r = (p >> 16) & 0xFF;
            BYTE a = (p >> 24) & 0xFF;
            fputc(b, fp);
            fputc(g, fp);
            fputc(r, fp);
            fputc(a, fp);
        }
    }

    fclose(fp);
    delete[] pixels;
    printf("图标已生成: %s\n", path);
}

int main() {
    GenerateIcon("app.ico");
    return 0;
}
