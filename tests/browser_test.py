"""End-to-end app test in headless Chromium: simulator link, guided exam, save, fault, reset, exam record.
Run: python3 tests/browser_test.py [path-to-html]   (default app/index.html; dist/BASIRA_app.html after bundling)"""
import asyncio, json, os, sys
from playwright.async_api import async_playwright
HTML = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", "app", "index.html"))
SHOT = os.environ.get("BASIRA_SHOT")

async def main():
    fails = []
    async with async_playwright() as p:
        b = await p.chromium.launch()
        pg = await b.new_page(viewport={"width": 1440, "height": 1000}, accept_downloads=True)
        errs = []
        pg.on("pageerror", lambda e: errs.append(str(e)))
        pg.on("console", lambda m: m.type == "error" and errs.append(m.text))
        await pg.goto("file://" + HTML)
        await pg.click("#connect")
        await pg.wait_for_function("window.BASIRA_APP.status && window.BASIRA_APP.status.st === 'READY'", timeout=5000)
        await pg.click("#demo")
        await pg.wait_for_function("document.getElementById('demo').disabled === false && window.BASIRA_APP.saved.R", timeout=60000)
        saved = await pg.evaluate("window.BASIRA_APP.saved.R")
        spec = saved["result"]["spectacle12"]
        # patient A right eye needs -2.25 / -0.75 x 175 (device plane); max-plus endpoint at 6 m then chart and vertex correction
        shown = await pg.inner_text("#rSpec")
        if shown.replace(" ", "") != "−2.25/−0.75×175°": fails.append(f"spectacle Rx shown {shown!r}, raw {spec}")
        if "calibrated" not in saved["result"] or saved["result"]["calibrated"] is not False: fails.append("nominal flag missing")
        if not await pg.is_visible("#calBanner"): fails.append("NOT CALIBRATED banner not shown")
        blur = await pg.inner_text("#kBlur")
        if float(blur.split()[0]) > 0.15: fails.append(f"residual blur at end of exam {blur}")
        if not saved["result"]["corneal"]: fails.append("corneal value missing")
        if SHOT: await pg.screenshot(path=SHOT, full_page=True)
        # fault -> unknown -> reset
        await pg.click("#fSensor")
        await pg.wait_for_function("window.BASIRA_APP.status.st === 'FAULT'", timeout=3000)
        if (await pg.inner_text("#rConf")) != "غير معروف": fails.append("result not marked unknown in FAULT")
        if not await pg.is_visible("#faultBanner"): fails.append("fault banner hidden")
        await pg.click("#fClear"); await pg.click("#reset")
        await pg.wait_for_function("window.BASIRA_APP.status.st === 'READY'", timeout=3000)
        # exam record download
        async with pg.expect_download() as dl:
            await pg.click("#download")
        d = await dl.value
        rec = json.load(open(await d.path()))
        if rec["exam"]["bvd_mm"] != 13 or "R" not in rec["exam"]["eyes"] or not rec["faults"]: fails.append("exam record incomplete")
        if errs: fails.append("console errors: " + "; ".join(errs[:3]))
        await b.close()
    print("browser test:", "PASS" if not fails else "FAIL", f"(spectacle R {spec['S']:+.2f} / {spec['C']:+.2f} x {spec['ax']:.0f}, blur {blur})")
    for f in fails: print("  FAIL", f)
    return 1 if fails else 0

sys.exit(asyncio.run(main()))
