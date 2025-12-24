#include <nan.h>
#include <windows.h>
#include <winspool.h>
#include <commdlg.h>
#include <cstring>
#include <algorithm>
#include "pdfium_wrapper.h"

using namespace v8;

// Windows printing functions - silently print to default printer
static bool PrintBitmapToPrinter(HBITMAP hBitmap, const wchar_t* printerName) {
    if (!hBitmap) {
        return false;
    }
    
    // Get default printer name if not provided
    wchar_t defaultPrinterName[256] = {0};
    DWORD nameLen = sizeof(defaultPrinterName) / sizeof(defaultPrinterName[0]);
    wchar_t printerNameBuffer[256] = {0};
    const wchar_t* actualPrinterName = printerName;
    
    if (!printerName) {
        if (!GetDefaultPrinterW(defaultPrinterName, &nameLen)) {
            return false;
        }
        actualPrinterName = defaultPrinterName;
    } else {
        wcscpy_s(printerNameBuffer, sizeof(printerNameBuffer) / sizeof(printerNameBuffer[0]), printerName);
        actualPrinterName = printerNameBuffer;
    }
    
    // Open printer (need non-const string for OpenPrinterW)
    HANDLE hPrinter = nullptr;
    PRINTER_DEFAULTSW printerDefaults = {0};
    printerDefaults.DesiredAccess = PRINTER_ACCESS_USE;
    
    wchar_t printerNameCopy[256] = {0};
    wcscpy_s(printerNameCopy, sizeof(printerNameCopy) / sizeof(printerNameCopy[0]), actualPrinterName);
    
    if (!OpenPrinterW(printerNameCopy, &hPrinter, &printerDefaults)) {
        return false;
    }
    
    // Get printer DC
    HDC hdcPrinter = nullptr;
    DOCINFOW di = {0};
    di.cbSize = sizeof(di);
    di.lpszDocName = L"PDF Print Job";
    
    bool success = false;
    
    // Use StartDoc/StartPage/EndPage/EndDoc for printing
    hdcPrinter = CreateDCW(L"WINSPOOL", printerNameCopy, nullptr, nullptr);
    if (!hdcPrinter) {
        ClosePrinter(hPrinter);
        return false;
    }
    
    if (StartDocW(hdcPrinter, &di) > 0) {
        if (StartPage(hdcPrinter) > 0) {
            // Get bitmap dimensions
            BITMAP bm;
            GetObject(hBitmap, sizeof(BITMAP), &bm);
            
            // Get printer page size and margins
            int pageWidth = GetDeviceCaps(hdcPrinter, PHYSICALWIDTH);
            int pageHeight = GetDeviceCaps(hdcPrinter, PHYSICALHEIGHT);
            int marginX = GetDeviceCaps(hdcPrinter, PHYSICALOFFSETX);
            int marginY = GetDeviceCaps(hdcPrinter, PHYSICALOFFSETY);
            int printableWidth = GetDeviceCaps(hdcPrinter, HORZRES);
            int printableHeight = GetDeviceCaps(hdcPrinter, VERTRES);
            
            // Validate dimensions
            if (bm.bmWidth <= 0 || bm.bmHeight <= 0 || 
                printableWidth <= 0 || printableHeight <= 0) {
                EndPage(hdcPrinter);
                EndDoc(hdcPrinter);
                DeleteDC(hdcPrinter);
                ClosePrinter(hPrinter);
                return false;
            }
            
            // Calculate scaling to fit printable area
            double scaleX = (double)printableWidth / bm.bmWidth;
            double scaleY = (double)printableHeight / bm.bmHeight;
            double scale = (scaleX < scaleY) ? scaleX : scaleY;
            
            int printWidth = (int)(bm.bmWidth * scale);
            int printHeight = (int)(bm.bmHeight * scale);
            int printX = marginX + (printableWidth - printWidth) / 2;
            int printY = marginY + (printableHeight - printHeight) / 2;
            
            // Create memory DC for bitmap
            HDC hdcMem = CreateCompatibleDC(hdcPrinter);
            if (hdcMem) {
                HGDIOBJ oldBitmap = SelectObject(hdcMem, hBitmap);
                
                // Use HALFTONE for better quality
                SetStretchBltMode(hdcPrinter, HALFTONE);
                SetBrushOrgEx(hdcPrinter, 0, 0, nullptr);
                
                // Blit bitmap to printer DC (reference WinUtil.cpp BlitHBITMAP)
                BOOL blitSuccess = StretchBlt(hdcPrinter, printX, printY, printWidth, printHeight,
                                             hdcMem, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);
                
                SelectObject(hdcMem, oldBitmap);
                DeleteDC(hdcMem);
                
                if (blitSuccess) {
                    success = true;
                }
            }
            
            EndPage(hdcPrinter);
        }
        EndDoc(hdcPrinter);
    }
    
    DeleteDC(hdcPrinter);
    ClosePrinter(hPrinter);
    
    return success;
}

static HBITMAP CreateHBITMAPFromBitmapData(BitmapData* bitmapData) {
    if (!bitmapData || !bitmapData->data || 
        bitmapData->width <= 0 || bitmapData->height <= 0) {
        return nullptr;
    }
    
    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = bitmapData->width;
    bmi.bmiHeader.biHeight = -bitmapData->height; // Negative for top-down DIB
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    
    void* bits = nullptr;
    HBITMAP hBitmap = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    
    if (!hBitmap || !bits) {
        if (hBitmap) {
            DeleteObject(hBitmap);
        }
        return nullptr;
    }
    
    // Copy bitmap data (BGRA format)
    unsigned char* dest = (unsigned char*)bits;
    unsigned char* src = bitmapData->data;
    int destStride = bitmapData->width * 4;
    
    for (int y = 0; y < bitmapData->height; y++) {
        memcpy(dest, src, destStride);
        dest += destStride;
        src += bitmapData->stride;
    }
    
    return hBitmap;
}

// Initialize pdfium library
NAN_METHOD(Initialize) {
    bool result = PdfiumWrapper::Initialize();
    info.GetReturnValue().Set(Nan::New(result));
}

// Load PDF file
NAN_METHOD(LoadPdf) {
    if (!info[0]->IsString()) {
        Nan::ThrowTypeError("Argument must be a string (file path)");
        return;
    }
    
    Nan::Utf8String path(info[0]);
    std::string filePath(*path);
    
    bool result = PdfiumWrapper::LoadPdf(filePath);
    info.GetReturnValue().Set(Nan::New(result));
}

// Get page count
NAN_METHOD(GetPageCount) {
    int count = PdfiumWrapper::GetPageCount();
    info.GetReturnValue().Set(Nan::New(count));
}

// Print PDF to default printer
NAN_METHOD(PrintPdf) {
    if (!info[0]->IsString()) {
        Nan::ThrowTypeError("Argument must be a string (file path)");
        return;
    }
    
    int dpi = 300; // Default DPI
    if (info[1]->IsNumber()) {
        dpi = Nan::To<int>(info[1]).FromJust();
        // Validate DPI range (72-1200 is reasonable)
        if (dpi < 72 || dpi > 1200) {
            Nan::ThrowRangeError("DPI must be between 72 and 1200");
            return;
        }
    }
    
    Nan::Utf8String path(info[0]);
    std::string filePath(*path);
    
    // Initialize pdfium
    if (!PdfiumWrapper::Initialize()) {
        Nan::ThrowError("Failed to initialize pdfium");
        return;
    }
    
    // Load PDF
    if (!PdfiumWrapper::LoadPdf(filePath)) {
        PdfiumWrapper::Shutdown();
        Nan::ThrowError("Failed to load PDF file");
        return;
    }
    
    int pageCount = PdfiumWrapper::GetPageCount();
    if (pageCount == 0) {
        PdfiumWrapper::Shutdown();
        Nan::ThrowError("PDF has no pages");
        return;
    }
    
    // Print each page
    bool success = true;
    for (int i = 0; i < pageCount; i++) {
        BitmapData* bitmap = PdfiumWrapper::RenderPageToBitmap(i, dpi);
        if (!bitmap) {
            success = false;
            break;
        }
        
        // Convert to HBITMAP
        HBITMAP hBitmap = CreateHBITMAPFromBitmapData(bitmap);
        if (!hBitmap) {
            PdfiumWrapper::FreeBitmap(bitmap);
            success = false;
            break;
        }
        
        // Print bitmap
        bool pageSuccess = PrintBitmapToPrinter(hBitmap, nullptr);
        DeleteObject(hBitmap);
        PdfiumWrapper::FreeBitmap(bitmap);
        
        if (!pageSuccess) {
            success = false;
            break;
        }
    }
    
    PdfiumWrapper::CloseDocument();
    PdfiumWrapper::Shutdown();
    
    info.GetReturnValue().Set(Nan::New(success));
}

// Module initialization
NAN_MODULE_INIT(Init) {
    Nan::SetMethod(target, "initialize", Initialize);
    Nan::SetMethod(target, "loadPdf", LoadPdf);
    Nan::SetMethod(target, "getPageCount", GetPageCount);
    Nan::SetMethod(target, "printPdf", PrintPdf);
}

NODE_MODULE(pdfprint, Init)

