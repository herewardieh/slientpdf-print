{
  "targets": [
    {
      "target_name": "pdfprint",
      "sources": [
        "src/pdfprint.cpp",
        "src/pdfium_wrapper.cpp"
      ],
      "include_dirs": [
        "<!@(node -p \"require('nan').include\")",
        "pdfium-prebuilt/pdfium-win-x64/include",
        "pdfium-prebuilt/pdfium-win-x64/include/cpp"
      ],
      "libraries": [
        "pdfium",
        "-lgdi32",
        "-lwinspool"
      ],
      "library_dirs": [
        "pdfium-prebuilt/pdfium-win-x64/lib"
      ],
      "defines": [
        "V8_DEPRECATION_WARNINGS=1",
        "PDF_ENABLE_V8=1"
      ],
      "conditions": [
        ["OS=='win'", {
          "msvs_settings": {
            "VCCLCompilerTool": {
              "ExceptionHandling": 1,
              "RuntimeLibrary": 2,
              "AdditionalOptions": ["/std:c++14"],
              "AdditionalIncludeDirectories": [
                "<!@(node -p \"require('nan').include\")",
                "pdfium-prebuilt/pdfium-win-x64/include",
                "pdfium-prebuilt/pdfium-win-x64/include/cpp"
              ]
            },
            "VCLinkerTool": {
              "AdditionalLibraryDirectories": [
                "pdfium-prebuilt/pdfium-win-x64/lib"
              ],
              "Machine": "MachineX64"
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
      ]
    }
  ]
}
