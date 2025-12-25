{
  "targets": [
    {
      "target_name": "pdfprint",
      "sources": [
        "src/pdfprint.cpp",
        "src/pdfium_win.cpp"
      ],
      "defines": [
        "NAPI_CPP_EXCEPTIONS",
        "V8_DEPRECATION_WARNINGS=1",
        "PDF_ENABLE_V8=1"
      ],
      "cflags!": ["-fno-exceptions"],
      "cflags_cc!": ["-fno-exceptions"],
      "conditions": [
        ["OS=='win' and target_arch=='arm64'", {
          "include_dirs": [
            "<!(node -p \"require('node-addon-api').include_dir\")",
            "pdfium-prebuilt/pdfium-win-arm64/include",
            "pdfium-prebuilt/pdfium-win-arm64/include/cpp"
          ],
          "library_dirs": [
            "pdfium-prebuilt/pdfium-win-arm64/lib"
          ],
          "msvs_settings": {
            "VCCLCompilerTool": {
              "ExceptionHandling": 1,
              "AdditionalIncludeDirectories": [
                "<!(node -p \"require('node-addon-api').include_dir\")",
                "..\\pdfium-prebuilt\\pdfium-win-arm64\\include",
                "..\\pdfium-prebuilt\\pdfium-win-arm64\\include\\cpp"
              ]
            },
            "VCLinkerTool": {
              "AdditionalLibraryDirectories": [
                "..\\pdfium-prebuilt\\pdfium-win-arm64\\lib"
              ]
            }
          },
          "copies": [
            {
              "destination": "<(PRODUCT_DIR)",
              "files": [
                "pdfium-prebuilt/pdfium-win-arm64/bin/pdfium.dll"
              ]
            }
          ]
        }],
        ["OS=='win' and target_arch=='x64'", {
          "include_dirs": [
            "<!(node -p \"require('node-addon-api').include_dir\")",
            "pdfium-prebuilt/pdfium-win-x64/include",
            "pdfium-prebuilt/pdfium-win-x64/include/cpp"
          ],
          "library_dirs": [
            "pdfium-prebuilt/pdfium-win-x64/lib"
          ],
          "msvs_settings": {
            "VCCLCompilerTool": {
              "ExceptionHandling": 1,
              "AdditionalIncludeDirectories": [
                "<!(node -p \"require('node-addon-api').include_dir\")",
                "..\\pdfium-prebuilt\\pdfium-win-x64\\include",
                "..\\pdfium-prebuilt\\pdfium-win-x64\\include\\cpp"
              ]
            },
            "VCLinkerTool": {
              "AdditionalLibraryDirectories": [
                "..\\pdfium-prebuilt\\pdfium-win-x64\\lib"
              ]
            }
          },
          "copies": [
            {
              "destination": "<(PRODUCT_DIR)",
              "files": [
                "pdfium-prebuilt/pdfium-win-x64/bin/pdfium.dll"
              ]
            }
          ]
        }]
      ],
      "libraries": [
        "pdfium.dll.lib",
        "-lgdi32",
        "-lwinspool"
      ]
    }
  ]
}
