// BASIRA clinician app: exam workflow on top of protocol v1. The device owns positions, optics model and
// confirmation; the app owns the clinical steps (refinement, chart-distance and vertex conversions, exam record).
(function () {
  "use strict";
  var O = BasiraOptics, T = BasiraTransports;
  var $ = function (id) { return document.getElementById(id); };
  var EYE_AR = { R: "العين اليمنى", L: "العين اليسرى" };
  var APP_VERSION = "0.1.0";

  var S = {
    tr: null, connected: false, info: null, status: null, nextId: 1,
    eye: "R", req: { R: { S: 0, C: 0, ax: 180 }, L: { S: 0, C: 0, ax: 180 } },
    applied: { R: null, L: null },       // effective Rx sent for the eye (after refinement)
    sentId: { R: 0, L: 0 },              // id of the last set command per eye
    confirmed: { R: null, L: null },     // last confirmed message per eye (valid only if its id = sentId)
    jcc: 0, fog: 0, saved: { R: null, L: null }, faults: [], log: [],
    patient: null
  };

  // ---------------- log ----------------
  function log(msg, data) {
    var t = new Date();
    S.log.push({ time: t.toISOString(), msg: msg, data: data || null });
    var d = document.createElement("div");
    d.innerHTML = '<span class="ltr">' + t.toLocaleTimeString("en-GB") + "</span> — " + msg;
    $("log").prepend(d);
  }

  // ---------------- link ----------------
  function send(obj) { if (S.tr && S.connected) { S.tr.send(obj); if ($("verbose").checked && obj.t !== "ping") log("→ " + esc(JSON.stringify(obj))); } }
  function cmd(obj) { obj.id = S.nextId++; send(obj); return obj.id; }
  function esc(s) { return String(s).replace(/[&<>]/g, function (c) { return { "&": "&amp;", "<": "&lt;", ">": "&gt;" }[c]; }); }

  async function connect() {
    if (S.connected) { disconnect(); return; }
    var mode = $("mode").value, tr;
    if (mode === "sim") tr = new T.SimTransport();
    else if (mode === "usb") tr = new T.SerialTransport(460800);
    else tr = new T.WsTransport(T.WsTransport.defaultUrl());
    tr.onmessage = onMessage;
    tr.onclose = function (why) { if (S.tr === tr) { S.connected = false; log("انقطع الاتصال: " + why); updateUi(); } };
    try {
      await tr.connect();
    } catch (e) { log("تعذّر الاتصال: " + esc(e.message || e)); return; }
    S.tr = tr; S.connected = true; S.info = null; S.status = null;
    S.confirmed = { R: null, L: null }; S.applied = { R: null, L: null };
    log("متصل: " + ({ sim: "محاكاة", usb: "USB", wifi: "Wi-Fi" }[mode]));
    send({ t: "hello" });
    updateUi();
  }
  function disconnect() {
    if (S.tr) { var tr = S.tr; S.tr = null; tr.close(); }
    S.connected = false; S.status = null; log("قُطع الاتصال"); updateUi();
  }
  setInterval(function () { if (S.connected) send({ t: "ping" }); }, 500);   // the device stops on silence during motion

  function onMessage(m) {
    if ($("verbose").checked && m.t !== "status") log("← " + esc(JSON.stringify(m)));
    switch (m.t) {
      case "info": S.info = m; log("الجهاز: " + esc(m.fw) + " — البروتوكول " + m.proto); break;
      case "status": S.status = m; break;
      case "ack": break;
      case "err": log("رفض الجهاز الأمر: " + esc(m.code) + " — " + esc(m.msg)); break;
      case "confirmed":
        S.confirmed[m.eye] = m;
        log("تأكيد " + EYE_AR[m.eye] + ": <span class='ltr'>" + O.fmtRx({ S: m.rx[0], C: m.rx[1], ax: m.rx[2] }, false) + "</span>" + (m.calibrated ? "" : " (معايرة اسمية)"));
        break;
      case "fault":
        S.faults.push({ time: new Date().toISOString(), code: m.code, axis: m.axis, msg: m.msg });
        S.confirmed = { R: null, L: null };
        log("<b>عطل " + esc(m.code) + "</b> — " + esc(m.axis) + ": " + esc(m.msg));
        break;
      case "calib": log("معايرة " + esc(m.eye) + ": " + esc(JSON.stringify(m.cal)) + (m.fitted ? " (مُعايرة)" : " (اسمية)")); break;
    }
    updateUi();
  }

  // ---------------- exam logic ----------------
  function effectiveRx(eye) {
    return O.pvToRx(O.composeRefinement(S.req[eye], S.jcc, S.fog));
  }
  function apply(why) {
    if (!S.connected) { log("غير متصل بالجهاز"); return; }
    var rx = effectiveRx(S.eye);
    S.applied[S.eye] = { req: Object.assign({}, S.req[S.eye]), jcc: S.jcc, fog: S.fog, rx: rx };
    S.sentId[S.eye] = cmd({ t: "set", eye: S.eye, S: +rx.S.toFixed(4), C: +rx.C.toFixed(4), ax: +rx.ax.toFixed(3) });
    log((why || "تطبيق") + " — " + EYE_AR[S.eye] + ": <span class='ltr'>" + O.fmtRx(rx, false) + "</span>");
    updateUi();
  }
  function confirmedFor(eye) {
    var c = S.confirmed[eye], st = S.status;
    if (!c || c.id !== S.sentId[eye] || !st || st.st === "FAULT" || !st[eye] || !st[eye].conf) return null;
    return c;
  }
  function results(eye) {
    var c = confirmedFor(eye);
    if (!c) return null;
    var bvd = parseFloat($("bvd").value), L = parseFloat($("dist").value) || 6;
    var pv = { M: c.pv[0] - 1 / L, J0: c.pv[1], J45: c.pv[2] };     // distance Rx = measured − 1/L
    var dist = O.pvToRx(pv), out = { device: { S: c.rx[0], C: c.rx[1], ax: c.rx[2] }, distance: dist, model: c.model, calibrated: c.calibrated, pos: c.pos };
    if (bvd > 0) { out.spectacle12 = O.vertexRx(dist, bvd, 12); out.corneal = O.vertexRx(dist, bvd, 0); }
    return out;
  }

  // ---------------- simulated patient ----------------
  var PATIENTS = {
    A: { R: { S: -2.25, C: -0.75, ax: 175 }, L: { S: -2.00, C: -0.50, ax: 10 } },
    B: { R: { S: +1.50, C: -1.25, ax: 90 }, L: { S: +1.75, C: -1.00, ax: 85 } },
    C: { R: { S: -4.00, C: -2.00, ax: 30 }, L: { S: -3.50, C: -2.50, ax: 150 } }
  };
  function q(x) { return Math.round(x * 4) / 4; }
  function randomEye() { return { S: q(-6 + Math.random() * 9), C: -q(Math.random() * 3), ax: 5 * Math.round(Math.random() * 35) + 5 }; }
  function setPatient(v) {
    S.patient = v === "X" ? { R: randomEye(), L: randomEye() } : PATIENTS[v];
    $("tR").textContent = O.fmtRx(S.patient.R, false); $("tL").textContent = O.fmtRx(S.patient.L, false);
  }
  // Residual after the device and the chart vergence; the eye accommodates up to amp (D) to clear excess minus.
  function patientBlur(eye) {
    if (!S.tr || S.tr.kind !== "sim" || !S.tr.dev) return null;
    var dev = S.tr.dev.truePower(eye), need = O.rxToPv(S.patient[eye]), L = parseFloat($("dist").value) || 6;
    var e = { M: dev.M - 1 / L - need.M, J0: dev.J0 - need.J0, J45: dev.J45 - need.J45 };
    var J = Math.hypot(e.J0, e.J45), e1 = e.M + J, e2 = e.M - J;           // principal meridional errors
    var amp = Math.max(0, parseFloat($("amp").value) || 0);
    var A = Math.min(amp, Math.max(0, -(e1 + e2) / 2));
    return { r1: e1 + A, r2: e2 + A, ang: Math.atan2(e.J45, e.J0) / 2 / O.DEG };
  }

  // ---------------- drawing ----------------
  var SVGNS = "http://www.w3.org/2000/svg", G = {};
  function el(tag, attrs, parent) { var e = document.createElementNS(SVGNS, tag); for (var k in attrs) e.setAttribute(k, attrs[k]); if (parent) parent.appendChild(e); return e; }
  function buildDevice() {
    var s = $("dev"); s.innerHTML = "";
    el("rect", { x: 0, y: 0, width: 360, height: 330, rx: 14, fill: "#F4F6F5" }, s);
    G.plate1 = el("rect", { x: 120, y: 60, width: 58, height: 200, rx: 6, fill: "#D3ECE8", stroke: "#0F766E", "stroke-width": 1.5, opacity: .85 }, s);
    G.plate2 = el("rect", { x: 182, y: 60, width: 58, height: 200, rx: 6, fill: "#D3ECE8", stroke: "#0F766E", "stroke-width": 1.5, opacity: .85 }, s);
    el("text", { x: 180, y: 300, "text-anchor": "middle", "font-size": 12, fill: "#4B5A63" }, s).textContent = "صفيحتا ألفاريز (الحركة مكبَّرة ×12 في الرسم)";
    el("circle", { cx: 180, cy: 160, r: 96, fill: "none", stroke: "#13212B", "stroke-width": 10, "stroke-dasharray": "3 4", opacity: .5 }, s);
    el("circle", { cx: 180, cy: 160, r: 84, fill: "rgba(255,255,255,.35)", stroke: "#13212B", "stroke-width": 2 }, s);
    G.axB = el("line", { x1: 96, y1: 160, x2: 264, y2: 160, stroke: "#B4410C", "stroke-width": 3, "stroke-dasharray": "8 5" }, s);
    G.axA = el("line", { x1: 96, y1: 160, x2: 264, y2: 160, stroke: "#0F766E", "stroke-width": 3 }, s);
    G.axR = el("line", { x1: 80, y1: 160, x2: 280, y2: 160, stroke: "#1D4ED8", "stroke-width": 1.5, "stroke-dasharray": "2 4" }, s);
    for (var a = 0; a < 180; a += 15) {
      var c = Math.cos(a * O.DEG), sn = Math.sin(a * O.DEG), r2 = a % 45 === 0 ? 106 : 101;
      el("line", { x1: 180 + 96 * c, y1: 160 - 96 * sn, x2: 180 + r2 * c, y2: 160 - r2 * sn, stroke: "#4B5A63", "stroke-width": 1.5 }, s);
      if (a % 45 === 0) el("text", { x: 180 + 118 * c, y: 164 - 118 * sn, "text-anchor": "middle", "font-size": 11, fill: "#4B5A63" }, s).textContent = a + "°";
    }
  }
  function axisLine(line, deg, r) {
    var c = Math.cos(deg * O.DEG), s = Math.sin(deg * O.DEG);
    line.setAttribute("x1", 180 - r * c); line.setAttribute("y1", 160 + r * s); line.setAttribute("x2", 180 + r * c); line.setAttribute("y2", 160 - r * s);
  }
  function sg(v, d) { return (v >= 0 ? "+" : "−") + Math.abs(v).toFixed(d === undefined ? 2 : d); }
  function m180(v) { return (((v % 180) + 180) % 180).toFixed(1) + "°"; }

  function updateUi() {
    var st = S.status, eye = S.eye, connected = S.connected;
    var state = connected ? (st ? st.st : "…") : "OFFLINE";
    $("stateBadge").className = "badge st-" + (connected && st ? st.st : "OFFLINE");
    $("stateText").textContent = state;
    $("link").textContent = connected ? ({ sim: "محاكاة", usb: "USB", wifi: "Wi-Fi" }[S.tr.kind]) : "غير متصل";
    $("link").className = "pill" + (connected ? " on" : "");
    $("connect").textContent = connected ? "قطع" : "اتصال";
    $("mode").disabled = connected;
    var isSim = connected ? S.tr.kind === "sim" : $("mode").value === "sim";
    ["simFaults", "patientCard", "patientSetup", "demo"].forEach(function (id) { $(id).classList.toggle("hidden", !isSim); });
    $("realPatientCard").classList.toggle("hidden", isSim);
    // banners
    var nominal = S.info && S.info.eyes && (!S.info.eyes.R.fitted || !S.info.eyes.L.fitted);
    $("calBanner").classList.toggle("show", !!(connected && nominal));
    var faulted = connected && st && st.st === "FAULT";
    $("faultBanner").classList.toggle("show", !!faulted);
    if (faulted) { var f = S.faults[S.faults.length - 1]; $("faultBanner").textContent = "عطل " + st.fault + (f ? " — " + f.axis + ": " + f.msg : "") + ". التصحيح غير معروف. أصلح السبب ثم اضغط «إعادة الضبط»."; }
    // controls
    var busy = !connected || !st || st.st === "MOVING" || st.st === "VERIFYING" || st.st === "FAULT" || st.st === "PARKED";
    $("apply").disabled = busy;
    ["jcc1", "jcc2", "jccOff", "fog"].forEach(function (id) { $(id).disabled = busy; });
    $("reset").disabled = !faulted; $("stop").disabled = !connected;
    $("jcc1").classList.toggle("on", S.jcc === 1); $("jcc2").classList.toggle("on", S.jcc === -1);
    $("fog").classList.toggle("on", S.fog > 0);
    var other = eye === "R" ? "L" : "R";
    $("occ").classList.toggle("on", !!(st && st[other] && st[other].occ));
    // requested values
    var r = S.req[eye];
    $("vS").textContent = sg(r.S); $("vC").textContent = sg(r.C); $("vA").textContent = Math.abs(r.C) < 0.01 ? "—" : (r.ax || 180) + "°";
    var a = S.applied[eye];
    var dirty = !a || a.req.S !== r.S || a.req.C !== r.C || a.req.ax !== r.ax || a.jcc !== S.jcc || a.fog !== S.fog;
    $("dirty").textContent = connected && dirty ? "تغييرات لم تُطبَّق بعد — اضغط «تطبيق الإعداد»." : "";
    $("devEye").textContent = EYE_AR[eye]; $("resEye").textContent = EYE_AR[eye];
    // device view
    if (st && st[eye]) {
      var p = st[eye].pos, ok = st[eye].ok;
      G.plate1.setAttribute("y", 60 - p[0] * 12); G.plate2.setAttribute("y", 60 + p[0] * 12);
      axisLine(G.axA, p[1], 84); axisLine(G.axB, p[2], 84);
      var cal = O.nominal(), rd = O.pvToRx(O.forward({ delta: p[0], alpha: p[1], beta: p[2] }, cal));
      if (Math.abs(rd.C) > 0.06) { G.axR.style.display = ""; axisLine(G.axR, rd.ax, 100); } else G.axR.style.display = "none";
      $("kD").textContent = sg(p[0], 3) + " mm"; $("kAl").textContent = m180(p[1]); $("kBe").textContent = m180(p[2]);
      $("kSens").textContent = ok.every(function (v) { return v; }) ? "OK" : "FAULT: " + ["plate", "ring+", "ring−"].filter(function (_, i) { return !ok[i]; }).join(", ");
      $("kOcc").textContent = (st.R.occ ? "closed" : "open") + " / " + (st.L.occ ? "closed" : "open");
    } else ["kD", "kAl", "kBe", "kSens", "kOcc"].forEach(function (id) { $(id).textContent = "—"; });
    $("kModel").textContent = S.info && S.info.eyes ? S.info.eyes[eye].model + (S.info.eyes[eye].fitted ? " (fitted)" : " (nominal)") : "—";
    $("kFw").textContent = S.info ? S.info.fw : "—";
    // results
    $("rReq").textContent = a ? O.fmtRx(a.rx, false) : "—";
    var res = results(eye);
    var pending = connected && st && (st.st === "MOVING" || st.st === "VERIFYING") && st.eye === eye;
    var unknown = connected && st && st.st === "FAULT";
    var blank = unknown ? "غير معروف" : pending ? "قيد التأكيد…" : "—";
    $("rConf").textContent = res ? O.fmtRx(res.device, false) + " @ " + ($("bvd").value || "?") + " mm" : blank;
    $("rDist").textContent = res ? O.fmtRx(res.distance, false) : blank;
    $("rSpec").textContent = res ? (res.spectacle12 ? O.fmtRx(res.spectacle12, true) : "أدخل مسافة القمة") : blank;
    $("rCorn").textContent = res ? (res.corneal ? O.fmtRx(res.corneal, false) : "أدخل مسافة القمة") : blank;
    // simulated patient view
    var b = patientBlur(eye);
    if (b) {
      var kpx = 7.5, sx = Math.min(22, Math.abs(b.r1) * kpx), sy = Math.min(22, Math.abs(b.r2) * kpx);
      $("gb").setAttribute("stdDeviation", sx.toFixed(2) + " " + sy.toFixed(2));
      $("rot").setAttribute("transform", "rotate(" + (-b.ang) + " 300 150)"); $("unrot").setAttribute("transform", "rotate(" + b.ang + " 300 150)");
      $("kBlur").textContent = Math.sqrt((b.r1 * b.r1 + b.r2 * b.r2) / 2).toFixed(2) + " D";
      var occThis = st && st[eye] && st[eye].occ;
      $("chartTag").textContent = "لوحة القراءة على " + ($("dist").value || 6) + " م — " + EYE_AR[eye] + (occThis ? " (هذه العين محجوبة!)" : "") + (S.jcc ? " — متقاطعة " + (S.jcc > 0 ? 1 : 2) : "") + (S.fog ? " — ضباب" : "");
      $("ampTxt").textContent = (parseFloat($("amp").value) || 0).toFixed(2) + " D";
    }
  }

  // ---------------- UI wiring ----------------
  document.querySelectorAll("button.sm").forEach(function (b) {
    b.addEventListener("click", function () {
      var kv = b.dataset.d.split(","), k = kv[0], v = +kv[1], r = S.req[S.eye];
      if (k === "S") r.S = Math.max(-12, Math.min(12, +(r.S + v).toFixed(2)));
      if (k === "C") r.C = Math.max(-6, Math.min(0, +(r.C + v).toFixed(2)));
      if (k === "A") r.ax = ((r.ax + v - 1) % 180 + 180) % 180 + 1;
      updateUi();
    });
  });
  $("connect").addEventListener("click", connect);
  $("mode").addEventListener("change", updateUi);
  $("apply").addEventListener("click", function () { S.jcc = 0; apply("تطبيق الإعداد"); });
  function selectEye(e) {
    S.eye = e; S.jcc = 0; S.fog = 0;
    $("eyeR").setAttribute("aria-pressed", e === "R"); $("eyeL").setAttribute("aria-pressed", e === "L");
    log("اختيار " + EYE_AR[e]); updateUi();
  }
  $("eyeR").addEventListener("click", function () { selectEye("R"); });
  $("eyeL").addEventListener("click", function () { selectEye("L"); });
  function jcc(v) {
    if (Math.abs(S.req[S.eye].C) < 0.01) { log("الأسطوانة المتقاطعة تحتاج محورًا: أضف أسطوانة أولًا."); return; }
    S.jcc = v; apply(v ? "أسطوانة متقاطعة — وضع " + (v > 0 ? 1 : 2) : "إلغاء الأسطوانة المتقاطعة");
  }
  $("jcc1").addEventListener("click", function () { jcc(1); });
  $("jcc2").addEventListener("click", function () { jcc(-1); });
  $("jccOff").addEventListener("click", function () { jcc(0); });
  $("fog").addEventListener("click", function () { S.fog = S.fog ? 0 : 1; apply(S.fog ? "ضباب +1.00" : "إزالة الضباب"); });
  $("occ").addEventListener("click", function () {
    var other = S.eye === "R" ? "L" : "R", on = !(S.status && S.status[other] && S.status[other].occ);
    cmd({ t: "occlude", eye: other, on: on }); log((on ? "حجب " : "فتح ") + EYE_AR[other]);
  });
  $("stop").addEventListener("click", function () { send({ t: "stop", id: S.nextId++ }); log("إيقاف فوري"); });
  $("reset").addEventListener("click", function () { cmd({ t: "reset" }); log("طلب إعادة الضبط"); });
  $("fSensor").addEventListener("click", function () { cmd({ t: "sim_fault", kind: "sensor" }); });
  $("fStall").addEventListener("click", function () { cmd({ t: "sim_fault", kind: "stall" }); });
  $("fLink").addEventListener("click", function () { cmd({ t: "sim_fault", kind: "link" }); });
  $("fClear").addEventListener("click", function () { cmd({ t: "sim_fault", kind: "clear" }); });
  $("save").addEventListener("click", function () {
    var bvd = parseFloat($("bvd").value), pd = parseFloat($("pd").value);
    if (!(bvd >= 8 && bvd <= 20) || !(pd >= 40 && pd <= 80)) { $("saved").textContent = "أدخل مسافة القمة (8–20 mm) و PD (40–80 mm) قبل الحفظ."; return; }
    if (S.jcc || S.fog) { $("saved").textContent = "ألغِ الأسطوانة المتقاطعة والضباب ثم طبّق الوصفة النهائية قبل الحفظ."; return; }
    var res = results(S.eye);
    if (!res) { $("saved").textContent = "لا يمكن الحفظ: يجب أن يؤكّد الجهاز الوصفة الحالية (CONFIRMED)."; return; }
    S.saved[S.eye] = { time: new Date().toISOString(), requested: S.applied[S.eye].rx, result: res };
    $("saved").innerHTML = "محفوظ — اليمنى: <span class='ltr'>" + (S.saved.R ? O.fmtRx(S.saved.R.result.spectacle12, true) : "—") + "</span> | اليسرى: <span class='ltr'>" + (S.saved.L ? O.fmtRx(S.saved.L.result.spectacle12, true) : "—") + "</span>";
    log("حفظ نتيجة " + EYE_AR[S.eye]);
  });
  $("download").addEventListener("click", function () {
    var rec = {
      record: "BASIRA exam", app_version: APP_VERSION, protocol: 1, created: new Date().toISOString(),
      link: S.tr ? S.tr.kind : null, device: S.info,
      exam: { bvd_mm: parseFloat($("bvd").value) || null, pd_mm: parseFloat($("pd").value) || null, chart_m: parseFloat($("dist").value) || null,
              complete: !!(S.saved.R && S.saved.L), eyes: S.saved },
      faults: S.faults, log: S.log
    };
    var blob = new Blob([JSON.stringify(rec, null, 1)], { type: "application/json" });
    var a = document.createElement("a");
    a.href = URL.createObjectURL(blob); a.download = "BASIRA_exam_" + rec.created.replace(/[:.]/g, "-") + ".json";
    document.body.appendChild(a); a.click(); a.remove();
    log("تنزيل سجل الفحص" + (rec.exam.complete ? "" : " (غير مكتمل)"));
  });
  $("sendRaw").addEventListener("click", function () {
    try { var o = JSON.parse($("raw").value); if (!o.id && o.t !== "ping") o.id = S.nextId++; send(o); log("أُرسلت: <span class='ltr'>" + esc(JSON.stringify(o)) + "</span>"); }
    catch (e) { log("رسالة غير صالحة: " + esc(e.message)); }
  });
  $("patient").addEventListener("change", function (e) { setPatient(e.target.value); log("مريض افتراضي جديد"); updateUi(); });
  $("reveal").addEventListener("click", function () { $("truth").classList.toggle("hidden"); });
  ["bvd", "pd", "dist", "amp"].forEach(function (id) { $(id).addEventListener("input", updateUi); });

  // ---------------- guided demo (simulation only) ----------------
  var sleep = function (ms) { return new Promise(function (r) { setTimeout(r, ms); }); };
  async function waitIdle() { for (var i = 0; i < 200; i++) { var s = S.status && S.status.st; if (s === "CONFIRMED" || s === "FAULT" || s === "READY") { if (i > 2) return; } await sleep(50); } }
  async function demo() {
    if (!S.connected) { $("mode").value = "sim"; await connect(); }
    if (S.tr.kind !== "sim") return;
    $("demo").disabled = true;
    if (S.status && S.status.st === "FAULT") { cmd({ t: "reset" }); await sleep(300); }
    $("patient").value = "A"; setPatient("A"); selectEye("R");
    $("bvd").value = 13; $("pd").value = 63; updateUi();
    var r = S.req.R; r.S = 0; r.C = 0; r.ax = 180;
    if (!(S.status && S.status.L && S.status.L.occ)) $("occ").click();
    await sleep(800);
    var steps = [-1.0, -2.0, -2.25];
    for (var i = 0; i < steps.length; i++) { r.S = steps[i]; apply("تعديل الكروي"); await waitIdle(); await sleep(900); }
    r.C = -0.5; apply("إضافة أسطوانة"); await waitIdle(); await sleep(900);
    jcc(1); await waitIdle(); await sleep(1000); jcc(-1); await waitIdle(); await sleep(1000);
    r.ax = 175; jcc(0); await waitIdle(); await sleep(800);
    r.C = -0.75; apply("تحسين الأسطوانة"); await waitIdle(); await sleep(1000);
    r.S = -2.00; apply("أقصى موجب بأفضل حدة (اللوحة على 6 م)"); await waitIdle(); await sleep(1000);
    $("save").click(); await sleep(800);
    log("انتهت الجولة.");
    $("demo").disabled = false;
  }
  $("demo").addEventListener("click", demo);

  buildDevice(); setPatient("A"); updateUi();
  setInterval(updateUi, 100);
  if (typeof window !== "undefined") window.BASIRA_APP = S;   // for tests and recording
})();
