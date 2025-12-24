#ifndef PDFIUM_WIN_H
#define PDFIUM_WIN_H

#include <string>
#include <vector>

/**
 * 位图数据结构
 * 用于存储渲染后的 PDF 页面位图数据
 */
struct BitmapData {
    unsigned char* data;  // 位图像素数据 (BGRA 格式)
    int width;            // 位图宽度（像素）
    int height;           // 位图高度（像素）
    int stride;           // 每行字节数
    int bitmapFormat;     // 格式：0 = BGRA, 1 = BGR
};

/**
 * PDFium 包装类
 * 提供 PDF 文档加载、渲染和位图转换功能
 */
class PdfiumWrapper {
public:
    /**
     * 初始化 pdfium 库
     * 在使用其他函数前必须先调用此函数
     * @return 成功返回 true
     */
    static bool Initialize();
    
    /**
     * 关闭 pdfium 库并清理资源
     * 关闭当前打开的文档并销毁 pdfium 库
     */
    static void Shutdown();
    
    /**
     * 加载 PDF 文件
     * @param filePath PDF 文件路径（UTF-8 编码）
     * @return 成功返回 true，失败返回 false
     */
    static bool LoadPdf(const std::string& filePath);
    
    /**
     * 获取已加载 PDF 的页数
     * @return 页数，如果未加载文档返回 0
     */
    static int GetPageCount();
    
    /**
     * 将指定页面渲染为位图
     * @param pageIndex 页面索引（从 0 开始）
     * @param dpi 渲染分辨率（每英寸点数，建议 300）
     * @return 位图数据指针，失败返回 nullptr。使用完后需调用 FreeBitmap 释放
     */
    static BitmapData* RenderPageToBitmap(int pageIndex, int dpi);
    
    /**
     * 释放位图数据
     * @param bitmap 位图数据指针（可为 nullptr）
     */
    static void FreeBitmap(BitmapData* bitmap);
    
    /**
     * 关闭当前打开的文档
     * 释放文档资源，但不销毁 pdfium 库
     */
    static void CloseDocument();
};

#endif // PDFIUM_WIN_H

