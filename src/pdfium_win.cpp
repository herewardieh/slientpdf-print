#include "pdfium_win.h"
#include "fpdfview.h"
#include "fpdf_doc.h"
#include <windows.h>
#include <string>
#include <memory>
#include <vector>
#include <cstdio>

static FPDF_DOCUMENT g_document = nullptr;

bool PdfiumWrapper::Initialize() {
    FPDF_InitLibrary();
    return true;
}

void PdfiumWrapper::Shutdown() {
    CloseDocument();
    FPDF_DestroyLibrary();
}

bool PdfiumWrapper::LoadPdf(const std::string& filePath) {
    CloseDocument();
    
    std::wstring wpath(filePath.begin(), filePath.end());
    FILE* file = _wfopen(wpath.c_str(), L"rb");
    if (!file) {
        return false;
    }
    
    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (fileSize <= 0) {
        fclose(file);
        return false;
    }
    
    std::vector<unsigned char> buffer(fileSize);
    size_t bytesRead = fread(buffer.data(), 1, fileSize, file);
    fclose(file);
    
    if (bytesRead != static_cast<size_t>(fileSize)) {
        return false;
    }
    
    g_document = FPDF_LoadMemDocument(buffer.data(), fileSize, nullptr);
    return g_document != nullptr;
}

int PdfiumWrapper::GetPageCount() {
    if (!g_document) {
        return 0;
    }
    return FPDF_GetPageCount(g_document);
}

BitmapData* PdfiumWrapper::RenderPageToBitmap(int pageIndex, int dpi) {
    if (!g_document) {
        return nullptr;
    }
    
    // Validate page index
    int pageCount = FPDF_GetPageCount(g_document);
    if (pageIndex < 0 || pageIndex >= pageCount) {
        return nullptr;
    }
    
    FPDF_PAGE page = FPDF_LoadPage(g_document, pageIndex);
    if (!page) {
        return nullptr;
    }
    
    // Calculate bitmap dimensions
    float width = FPDF_GetPageWidthF(page);
    float height = FPDF_GetPageHeightF(page);
    
    // Convert points to pixels at given DPI (72 points = 1 inch)
    int pixelWidth = (int)(width * dpi / 72.0f);
    int pixelHeight = (int)(height * dpi / 72.0f);
    
    // Validate dimensions
    if (pixelWidth <= 0 || pixelHeight <= 0) {
        FPDF_ClosePage(page);
        return nullptr;
    }
    
    // Create bitmap with 4 bytes per pixel (BGRA)
    int stride = pixelWidth * 4;
    unsigned char* bitmapBuffer = new (std::nothrow) unsigned char[stride * pixelHeight];
    if (!bitmapBuffer) {
        FPDF_ClosePage(page);
        return nullptr;
    }
    
    // Create FPDF bitmap
    FPDF_BITMAP bitmap = FPDFBitmap_CreateEx(pixelWidth, pixelHeight, FPDFBitmap_BGRA, 
                                              bitmapBuffer, stride);
    if (!bitmap) {
        FPDF_ClosePage(page);
        delete[] bitmapBuffer;
        return nullptr;
    }
    
    // Fill with white background
    FPDFBitmap_FillRect(bitmap, 0, 0, pixelWidth, pixelHeight, 0xFFFFFFFF);
    
    // Render page to bitmap
    // Parameters: bitmap, page, start_x, start_y, size_x, size_y, rotate, flags
    FPDF_RenderPageBitmap(bitmap, page, 0, 0, pixelWidth, pixelHeight, 0, 
                         FPDF_ANNOT | FPDF_LCD_TEXT | FPDF_NO_CATCH);
    
    BitmapData* result = new BitmapData();
    result->data = bitmapBuffer;
    result->width = pixelWidth;
    result->height = pixelHeight;
    result->stride = stride;
    result->bitmapFormat = 0; // BGRA
    
    FPDFBitmap_Destroy(bitmap);
    FPDF_ClosePage(page);
    
    return result;
}

void PdfiumWrapper::FreeBitmap(BitmapData* bitmap) {
    if (bitmap && bitmap->data) {
        delete[] bitmap->data;
        delete bitmap;
    }
}

void PdfiumWrapper::CloseDocument() {
    if (g_document) {
        FPDF_CloseDocument(g_document);
        g_document = nullptr;
    }
}

