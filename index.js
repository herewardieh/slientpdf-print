const pdfprint = require("./build/Release/pdfprint.node");

/**
 * Initialize the pdfium library
 * @returns {boolean} True if initialization successful
 */
function initialize() {
  return pdfprint.initialize();
}

/**
 * Load a PDF file
 * @param {string} filePath - Path to the PDF file
 * @returns {boolean} True if PDF loaded successfully
 */
function loadPdf(filePath) {
  return pdfprint.loadPdf(filePath);
}

/**
 * Get the number of pages in the loaded PDF
 * @returns {number} Number of pages
 */
function getPageCount() {
  return pdfprint.getPageCount();
}

/**
 * Print a PDF file to the default printer
 * @param {string} filePath - Path to the PDF file
 * @param {number} [dpi=300] - DPI for rendering (default: 300)
 * @returns {boolean} True if printing was successful
 */
function printPdf(filePath, dpi = 300) {
  return pdfprint.printPdf(filePath, dpi);
}

module.exports = {
  initialize,
  loadPdf,
  getPageCount,
  printPdf,
};
