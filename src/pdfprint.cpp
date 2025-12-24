#include <napi.h>
#include <windows.h>
#include <winspool.h>
#include <commdlg.h>
#include <cstring>
#include <algorithm>
#include "pdfium_win.h"

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
Napi::Value Initialize(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    bool result = PdfiumWrapper::Initialize();
    return Napi::Boolean::New(env, result);
}

// Load PDF file
Napi::Value LoadPdf(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    if (!info[0].IsString()) {
        Napi::TypeError::New(env, "Argument must be a string (file path)").ThrowAsJavaScriptException();
        return env.Null();
    }
    
    std::string filePath = info[0].As<Napi::String>().Utf8Value();
    bool result = PdfiumWrapper::LoadPdf(filePath);
    return Napi::Boolean::New(env, result);
}

// Get page count
Napi::Value GetPageCount(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    int count = PdfiumWrapper::GetPageCount();
    return Napi::Number::New(env, count);
}

// Print PDF to default printer
Napi::Value PrintPdf(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    if (!info[0].IsString()) {
        Napi::TypeError::New(env, "Argument must be a string (file path)").ThrowAsJavaScriptException();
        return env.Null();
    }
    
    int dpi = 300; // Default DPI
    if (info.Length() > 1 && info[1].IsNumber()) {
        dpi = info[1].As<Napi::Number>().Int32Value();
        // Validate DPI range (72-1200 is reasonable)
        if (dpi < 72 || dpi > 1200) {
            Napi::RangeError::New(env, "DPI must be between 72 and 1200").ThrowAsJavaScriptException();
            return env.Null();
        }
    }
    
    std::string filePath = info[0].As<Napi::String>().Utf8Value();
    
    // Initialize pdfium
    if (!PdfiumWrapper::Initialize()) {
        Napi::Error::New(env, "Failed to initialize pdfium").ThrowAsJavaScriptException();
        return env.Null();
    }
    
    // Load PDF
    if (!PdfiumWrapper::LoadPdf(filePath)) {
        PdfiumWrapper::Shutdown();
        Napi::Error::New(env, "Failed to load PDF file").ThrowAsJavaScriptException();
        return env.Null();
    }
    
    int pageCount = PdfiumWrapper::GetPageCount();
    if (pageCount == 0) {
        PdfiumWrapper::Shutdown();
        Napi::Error::New(env, "PDF has no pages").ThrowAsJavaScriptException();
        return env.Null();
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
    
    return Napi::Boolean::New(env, success);
}

// Module initialization
Napi::Object Init(Napi::Env env, Napi::Object exports) {
    exports.Set(Napi::String::New(env, "initialize"), Napi::Function::New(env, Initialize));
    exports.Set(Napi::String::New(env, "loadPdf"), Napi::Function::New(env, LoadPdf));
    exports.Set(Napi::String::New(env, "getPageCount"), Napi::Function::New(env, GetPageCount));
    exports.Set(Napi::String::New(env, "printPdf"), Napi::Function::New(env, PrintPdf));
    return exports;
}

NODE_API_MODULE(pdfprint, Init)

