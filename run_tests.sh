#!/usr/bin/env bash
# Runs every automated check in the repository. Optional parts are skipped (not failed) when their tool is missing.
set -u
cd "$(dirname "$0")"
pass=0; fail=0; skip=0
run() { local name="$1"; shift; echo "== $name"; if "$@"; then pass=$((pass+1)); else echo "!! FAILED: $name"; fail=$((fail+1)); fi; }
skip() { echo "== $1: skipped ($2)"; skip=$((skip+1)); }

run "physics: reference optics model" python3 physics/test_basira_optics.py
run "physics: optical design checks" sh -c 'python3 physics/verify_optics.py > /dev/null'
run "golden vectors are current" sh -c 'cp tests/golden_vectors.json /tmp/basira_golden_old.json && python3 tools/make_golden.py > /dev/null && cmp -s tests/golden_vectors.json /tmp/basira_golden_old.json'
if command -v g++ > /dev/null && command -v make > /dev/null; then run "firmware core on host (C++)" make -s -C firmware/test_host; else skip "firmware core on host" "no g++/make"; fi
if command -v node > /dev/null; then run "app: JS optics + simulated device" node tests/app_test.js; else skip "app tests" "no node"; fi
run "calibration fit self-test" sh -c 'cd calibration && python3 fit_calibration.py --selftest'
run "usability scoring (SUS)" python3 validation/usability_summary.py --selftest
run "validation analysis (synthetic example)" sh -c 'cd validation && python3 clinical_agreement.py --make-example > /dev/null'
if python3 -c "import playwright" 2> /dev/null; then
  run "app bundle" python3 tools/build_app_bundle.py
  run "app end to end in a browser" python3 tests/browser_test.py dist/BASIRA_app.html
else skip "browser test" "pip install playwright && python3 -m playwright install chromium"; fi
if command -v arduino-cli > /dev/null; then run "firmware compiles for ESP32-S3" arduino-cli compile --fqbn esp32:esp32:esp32s3:PSRAM=opi,FlashSize=8M firmware/basira_fw
else skip "ESP32 compile" "no arduino-cli (see firmware/README.md)"; fi
echo; echo "passed $pass, failed $fail, skipped $skip"
[ "$fail" -eq 0 ]
