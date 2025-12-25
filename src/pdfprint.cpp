#include <napi.h>
#include <windows.h>
#include <winspool.h>
#include <commdlg.h>
#include <cstring>
#include <algorithm>
#include <sstream>
#include "pdfium_win.h"

// Windows printing functions - silently print to default printer
static void PrintBitmapToPrinter(HBITMAP hBitmap, const wchar_t* printerName, Napi::Env env) {
    if (!hBitmap) {
        throw Napi::Error::New(env, "PrintBitmapToPrinter: hBitmap is null");
    }
    
    // Get default printer name if not provided
    wchar_t defaultPrinterName[256] = {0};
    DWORD nameLen = sizeof(defaultPrinterName) / sizeof(defaultPrinterName[0]);
    wchar_t printerNameBuffer[256] = {0};
    const wchar_t* actualPrinterName = printerName;
    
    if (!printerName) {
        if (!GetDefaultPrinterW(defaultPrinterName, &nameLen)) {
            DWORD errorCode = GetLastError();
            std::string errorMsg = "Failed to get default printer, error code: " + std::to_string(errorCode);
            throw Napi::Error::New(env, errorMsg);
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
        DWORD errorCode = GetLastError();
        std::string errorMsg = "Failed to open printer, error code: " + std::to_string(errorCode);
        throw Napi::Error::New(env, errorMsg);
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
        DWORD errorCode = GetLastError();
        ClosePrinter(hPrinter);
        std::string errorMsg = "Failed to create printer DC, error code: " + std::to_string(errorCode);
        throw Napi::Error::New(env, errorMsg);
    }
    
    int docResult = StartDocW(hdcPrinter, &di);
    if (docResult <= 0) {
        DWORD errorCode = GetLastError();
        DeleteDC(hdcPrinter);
        ClosePrinter(hPrinter);
        std::string errorMsg = "Failed to start print job (StartDocW), error code: " + std::to_string(errorCode);
        throw Napi::Error::New(env, errorMsg);
    }
    
    int pageResult = StartPage(hdcPrinter);
    if (pageResult <= 0) {
        DWORD errorCode = GetLastError();
        EndDoc(hdcPrinter);
        DeleteDC(hdcPrinter);
        ClosePrinter(hPrinter);
        std::string errorMsg = "Failed to start page (StartPage), error code: " + std::to_string(errorCode);
        throw Napi::Error::New(env, errorMsg);
    }
    
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
        std::string errorMsg = "Invalid bitmap or printer dimensions (bitmap: " + 
                               std::to_string(bm.bmWidth) + "x" + std::to_string(bm.bmHeight) +
                               ", printable: " + std::to_string(printableWidth) + "x" + std::to_string(printableHeight) + ")";
        throw Napi::Error::New(env, errorMsg);
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
    if (!hdcMem) {
        DWORD errorCode = GetLastError();
        EndPage(hdcPrinter);
        EndDoc(hdcPrinter);
        DeleteDC(hdcPrinter);
        ClosePrinter(hPrinter);
        std::string errorMsg = "Failed to create memory DC, error code: " + std::to_string(errorCode);
        throw Napi::Error::New(env, errorMsg);
    }
    
    HGDIOBJ oldBitmap = SelectObject(hdcMem, hBitmap);
    
    // Use HALFTONE for better quality
    SetStretchBltMode(hdcPrinter, HALFTONE);
    SetBrushOrgEx(hdcPrinter, 0, 0, nullptr);
    
    // Blit bitmap to printer DC (reference WinUtil.cpp BlitHBITMAP)
    BOOL blitSuccess = StretchBlt(hdcPrinter, printX, printY, printWidth, printHeight,
                                 hdcMem, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);
    
    SelectObject(hdcMem, oldBitmap);
    DeleteDC(hdcMem);
    
    if (!blitSuccess) {
        DWORD errorCode = GetLastError();
        EndPage(hdcPrinter);
        EndDoc(hdcPrinter);
        DeleteDC(hdcPrinter);
        ClosePrinter(hPrinter);
        std::string errorMsg = "Failed to blit bitmap to printer, error code: " + std::to_string(errorCode);
        throw Napi::Error::New(env, errorMsg);
    }
    
    EndPage(hdcPrinter);
    EndDoc(hdcPrinter);
    DeleteDC(hdcPrinter);
    ClosePrinter(hPrinter);
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
        throw Napi::Error::New(env, "Failed to initialize pdfium");
    }
    
    try {
        // Load PDF
        if (!PdfiumWrapper::LoadPdf(filePath)) {
            PdfiumWrapper::Shutdown();
            throw Napi::Error::New(env, "Failed to load PDF file: " + filePath);
        }
        
        int pageCount = PdfiumWrapper::GetPageCount();
        if (pageCount == 0) {
            PdfiumWrapper::Shutdown();
            throw Napi::Error::New(env, "PDF has no pages");
        }
        
        // Print each page
        for (int i = 0; i < pageCount; i++) {
            BitmapData* bitmap = nullptr;
            HBITMAP hBitmap = nullptr;
            
            try {
                // Render page to bitmap
                bitmap = PdfiumWrapper::RenderPageToBitmap(i, dpi);
                if (!bitmap) {
                    throw Napi::Error::New(env, "Failed to render page " + std::to_string(i + 1) + " to bitmap");
                }
                
                // Convert to HBITMAP
                hBitmap = CreateHBITMAPFromBitmapData(bitmap);
                if (!hBitmap) {
                    PdfiumWrapper::FreeBitmap(bitmap);
                    throw Napi::Error::New(env, "Failed to create HBITMAP for page " + std::to_string(i + 1));
                }
                
                // Print bitmap
                PrintBitmapToPrinter(hBitmap, nullptr, env);
                
                // Clean up
                DeleteObject(hBitmap);
                PdfiumWrapper::FreeBitmap(bitmap);
                
            } catch (const Napi::Error& e) {
                // Clean up resources before rethrowing
                if (hBitmap) {
                    DeleteObject(hBitmap);
                }
                if (bitmap) {
                    PdfiumWrapper::FreeBitmap(bitmap);
                }
                PdfiumWrapper::CloseDocument();
                PdfiumWrapper::Shutdown();
                throw;
            } catch (const std::exception& e) {
                // Clean up resources before throwing
                if (hBitmap) {
                    DeleteObject(hBitmap);
                }
                if (bitmap) {
                    PdfiumWrapper::FreeBitmap(bitmap);
                }
                PdfiumWrapper::CloseDocument();
                PdfiumWrapper::Shutdown();
                throw Napi::Error::New(env, "Exception while printing page " + std::to_string(i + 1) + ": " + std::string(e.what()));
            } catch (...) {
                // Clean up resources before throwing
                if (hBitmap) {
                    DeleteObject(hBitmap);
                }
                if (bitmap) {
                    PdfiumWrapper::FreeBitmap(bitmap);
                }
                PdfiumWrapper::CloseDocument();
                PdfiumWrapper::Shutdown();
                throw Napi::Error::New(env, "Unknown exception while printing page " + std::to_string(i + 1));
            }
        }
        
        PdfiumWrapper::CloseDocument();
        PdfiumWrapper::Shutdown();
        
        return Napi::Boolean::New(env, true);
        
    } catch (const Napi::Error& e) {
        // Re-throw NAPI errors as-is
        throw;
    } catch (const std::exception& e) {
        PdfiumWrapper::Shutdown();
        throw Napi::Error::New(env, "Exception during PDF processing: " + std::string(e.what()));
    } catch (...) {
        PdfiumWrapper::Shutdown();
        throw Napi::Error::New(env, "Unknown exception during PDF processing");
    }
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

