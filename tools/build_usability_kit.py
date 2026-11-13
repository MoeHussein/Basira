"""Render docs/usability_test_kit.html to submission/BASIRA_usability_test_kit.pdf (A4, running header and page numbers).
Needs: pip install playwright && python3 -m playwright install chromium; the Amiri font installed (OFL, github.com/aliftype/amiri)."""
import asyncio, os
from playwright.async_api import async_playwright
ROOT = os.path.normpath(os.path.join(os.path.dirname(__file__), ".."))
HEAD = ('<div dir="rtl" style="width:100%;font-family:Amiri,serif;font-size:9pt;color:#333;padding:0 20mm;display:flex;'
        'justify-content:space-between;border-bottom:.5pt solid #555;padding-bottom:1.5mm;margin:0 20mm">'
        '<span>نظام بصيرة — بروتوكول اختبار قابلية الاستخدام</span><span dir="ltr">BASIRA-UT-PRO-001 · Rev. 1.0</span></div>')
FOOT = ('<div dir="rtl" style="width:100%;font-family:Amiri,serif;font-size:9pt;color:#333;display:flex;'
        'justify-content:space-between;margin:0 20mm">'
        '<span>داخلي — مسودة للمراجعة</span><span>صفحة <span class="pageNumber"></span> من <span class="totalPages"></span></span></div>')

async def main():
    async with async_playwright() as p:
        b = await p.chromium.launch(); pg = await b.new_page()
        await pg.goto("file://" + os.path.join(ROOT, "docs", "usability_test_kit.html"))
        await pg.evaluate("document.fonts.ready")
        out = os.path.join(ROOT, "submission", "BASIRA_usability_test_kit.pdf")
        await pg.pdf(path=out, format="A4", print_background=True, display_header_footer=True, header_template=HEAD, footer_template=FOOT,
                     margin={"top": "24mm", "bottom": "20mm", "left": "20mm", "right": "20mm"})
        await b.close()
        print("wrote", out)
asyncio.run(main())
