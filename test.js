const pdfprint = require("./index.js");
const path = require("path");
const fs = require("fs");

/**
 * 测试 PDF 打印模块
 */

// 测试用的 PDF 文件路径（请根据实际情况修改）
const testPdfPath = process.argv[2] || "test.pdf";

console.log("==========================================");
console.log("PDF 打印模块测试");
console.log("==========================================");

// 检查 PDF 文件是否存在
if (!fs.existsSync(testPdfPath)) {
  console.error(`\n❌ 错误: PDF 文件不存在: ${testPdfPath}`);
  console.log("\n使用方法: node test.js [pdf文件路径]");
  console.log("示例: node test.js C:\\path\\to\\test.pdf");
  process.exit(1);
}

console.log(`\n📄 测试文件: ${path.resolve(testPdfPath)}`);

try {
  // 测试 1: 初始化 pdfium
  console.log("\n[测试 1] 初始化 pdfium 库...");
  const initResult = pdfprint.initialize();
  if (initResult) {
    console.log("✅ pdfium 初始化成功");
  } else {
    console.error("❌ pdfium 初始化失败");
    process.exit(1);
  }

  // 测试 2: 加载 PDF 文件
  console.log("\n[测试 2] 加载 PDF 文件...");
  const loadResult = pdfprint.loadPdf(testPdfPath);
  if (loadResult) {
    console.log("✅ PDF 文件加载成功");
  } else {
    console.error("❌ PDF 文件加载失败");
    console.error("   请检查文件路径是否正确，文件是否有效");
    process.exit(1);
  }

  // 测试 3: 获取页数
  console.log("\n[测试 3] 获取 PDF 页数...");
  const pageCount = pdfprint.getPageCount();
  if (pageCount > 0) {
    console.log(`✅ PDF 包含 ${pageCount} 页`);
  } else {
    console.error("❌ 无法获取页数或 PDF 为空");
    process.exit(1);
  }

  // 测试 4: 打印 PDF
  console.log("\n[测试 4] 打印 PDF 到默认打印机...");
  console.log("   注意: 确保已连接并配置了默认打印机");
  console.log("   DPI: 300 (默认)");

  try {
    const dpi = 300;
    const printResult = pdfprint.printPdf(testPdfPath, dpi);

    if (printResult) {
      console.log("✅ PDF 打印成功");
      console.log(`   已发送 ${pageCount} 页到默认打印机`);
    } else {
      console.error("❌ PDF 打印失败 (返回 false)");
      console.error("   可能的原因:");
      console.error("   - 没有配置默认打印机");
      console.error("   - 打印机不可用或离线");
      console.error("   - 打印权限不足");
      process.exit(1);
    }
  } catch (error) {
    console.error("\n❌ PDF 打印时发生异常:");
    console.error("   错误类型:", error.name || "Error");
    console.error("   错误消息:", error.message || String(error));
    if (error.stack) {
      console.error("\n   错误堆栈:");
      const stackLines = error.stack.split("\n");
      stackLines.forEach((line, index) => {
        if (index === 0) {
          console.error(`   ${line}`);
        } else {
          console.error(`   ${line.trim()}`);
        }
      });
    }
    console.error("\n   可能的原因:");
    console.error("   - 没有配置默认打印机");
    console.error("   - 打印机不可用或离线");
    console.error("   - 打印权限不足");
    console.error("   - PDF 文件损坏或格式不正确");
    console.error("   - 渲染页面时出错");
    console.error("   - 系统资源不足");
    process.exit(1);
  }

  console.log("\n==========================================");
  console.log("✅ 所有测试通过!");
  console.log("==========================================");
} catch (error) {
  console.error("\n❌ 测试过程中发生错误:");
  console.error(error);
  process.exit(1);
}
