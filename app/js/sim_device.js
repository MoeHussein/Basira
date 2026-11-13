// Simulated BASIRA device: answers protocol v1 (docs/PROTOCOL.md) like the firmware, so the app runs unchanged
// with or without hardware. Kinematic model (constant-speed drives, approach from below, sensor noise),
// not a copy of the firmware's control loop. Extra sim-only message: {"t":"sim_fault","kind":"sensor"|"stall"|"link"}.
var BasiraSimDevice = (function () {
  "use strict";
  var O = (typeof BasiraOptics !== "undefined") ? BasiraOptics : require("./optics.js");
  var AX = ["plate", "ringP", "ringM"], EYES = ["R", "L"];
  var SPEED = [6.0, 120, 120];          // mm/s, deg/s, deg/s
  var TOL = [0.010, 0.3, 0.3], BACKLASH = [0.10, 2.0, 2.0], NOISE = [0.002, 0.05, 0.05];
  var VERIFY_TOL = [0.030, 0.6, 0.6], VERIFY_MS = 150, MOVE_TIMEOUT_MS = 4000, LINK_TIMEOUT_MS = 1500;

  function SimDevice(opts) {
    opts = opts || {};
    this.now = 0;
    this.onLine = null;                // function(jsonLine)
    this.cfg = { R: { cal: O.nominal(), fitted: false }, L: { cal: O.nominal(), fitted: false } };
    this.state = "PARKED"; this.fault = null; this.eye = "R"; this.moveId = 0;
    this.pos = { R: [0, 45, 45], L: [0, 45, 45] };       // true positions
    this.target = { R: [0, 45, 45], L: [0, 45, 45] };
    this.aim = { R: [0, 45, 45], L: [0, 45, 45] };
    this.conf = { R: false, L: false }; this.occ = { R: false, L: false };
    this.inject = { sensor: false, stall: false };
    this.lastLink = 0; this.linkSeen = false; this.lastStatus = 0;
    this.seed = opts.seed || 12345;
    this.timer = null;
  }
  var P = SimDevice.prototype;
  P.rand = function () { this.seed = (this.seed * 16807) % 2147483647; return this.seed / 2147483647 * 2 - 1; };
  P.emit = function (o) { if (this.onLine) this.onLine(JSON.stringify(o)); };
  P.start = function () {
    var self = this, last = Date.now();
    this.emit(this.info());
    var loop = function () {
      var t = Date.now(), dt = Math.min(100, t - last); last = t;
      self.step(dt);
      self.timer = setTimeout(loop, 10);
    };
    this.timer = setTimeout(loop, 10);
  };
  P.stop = function () { if (this.timer) clearTimeout(this.timer); this.timer = null; };

  P.info = function () {
    return { t: "info", fw: "sim-0.1.0", proto: 1, sim: true, allow_nominal: true,
             eyes: { R: { model: this.cfg.R.cal.model_id, fitted: this.cfg.R.fitted }, L: { model: this.cfg.L.cal.model_id, fitted: this.cfg.L.fitted } } };
  };
  P.sensed = function (eye) {
    var p = this.pos[eye], self = this;
    return p.map(function (v, j) { return v + self.rand() * NOISE[j]; });
  };
  P.status = function () {
    var self = this, o = { t: "status", st: this.state, eye: this.eye };
    if (this.state === "FAULT") o.fault = this.fault;
    EYES.forEach(function (e) {
      var s = self.sensed(e), ok = [1, 1, 1];
      if (self.inject.sensor && e === "R") { ok[1] = 0; }
      var raw = [((s[0] / 5.0 * 180 / Math.PI) % 360 + 360) % 360, (s[1] % 360 + 360) % 360, (s[2] % 360 + 360) % 360];  // nominal mapping, r = 5 mm, zeros 0
      o[e] = { pos: s.map(function (v) { return +v.toFixed(4); }), raw: raw.map(function (v) { return +v.toFixed(4); }), ok: ok, occ: self.occ[e] ? 1 : 0, conf: self.conf[e] ? 1 : 0 };
    });
    return o;
  };
  P.err = function (id, code, msg) { this.emit({ t: "err", id: id, code: code, msg: msg }); };
  P.setState = function (s) { if (s !== this.state) { this.state = s; this.emit(this.status()); } };
  P.doFault = function (code, axis, msg) {
    this.state = "FAULT"; this.fault = code; this.conf.R = this.conf.L = false;
    this.emit({ t: "fault", code: code, axis: axis, msg: msg });
    this.emit(this.status());
  };
  P.busy = function (id) {
    if (this.state === "FAULT") { this.err(id, "FAULT", "device is in FAULT; send reset first"); return true; }
    if (this.state === "MOVING" || this.state === "VERIFYING") { this.err(id, "BUSY", "a move is in progress"); return true; }
    if (this.state === "PARKED") { this.err(id, "NOT_READY", "sensors not read yet"); return true; }
    return false;
  };
  P.startMove = function (id, eye, t) {
    var cur = this.pos[eye];
    this.target[eye] = t.slice();
    this.aim[eye] = t.map(function (v, j) { return v < cur[j] ? v - BACKLASH[j] : v; });   // approach from below
    this.conf[eye] = false; this.eye = eye; this.moveId = id; this.moveStart = this.now;
    this.emit({ t: "ack", id: id });
    this.setState("MOVING");
  };
  P.receive = function (line) {
    var m;
    try { m = JSON.parse(line); } catch (e) { this.err(0, "BAD_JSON", "could not parse message"); return; }
    this.linkSeen = true; this.lastLink = this.now;
    var id = m.id || 0, eye = m.eye;
    switch (m.t) {
      case "ping": return;
      case "hello": this.emit(this.info()); this.emit(this.status()); return;
      case "status": this.emit(this.status()); return;
      case "stop": this.emit({ t: "ack", id: id }); this.doFault("USER_STOP", "-", "stopped by the operator"); return;
      case "reset":
        if (this.inject.sensor) { this.err(id, "SENSOR", "R.ringP"); return; }
        this.inject.stall = false; this.fault = null; this.conf.R = this.conf.L = false;
        this.emit({ t: "ack", id: id }); this.state = "FAULT"; this.setState("READY"); return;
      case "occlude":
        if (eye !== "R" && eye !== "L") { this.err(id, "BAD_ARG", "need eye"); return; }
        this.occ[eye] = !!m.on; this.emit({ t: "ack", id: id }); return;
      case "set": {
        if (eye !== "R" && eye !== "L" || typeof m.S !== "number" || typeof m.C !== "number" || typeof m.ax !== "number") { this.err(id, "BAD_ARG", "need eye, S, C, ax"); return; }
        if (this.busy(id)) return;
        if (!(m.C <= 0)) { this.err(id, "BAD_RX", "expect minus cylinder: C <= 0"); return; }
        var p = this.pos[eye], r = O.inverse(O.rxToPv({ S: m.S, C: m.C, ax: m.ax }), this.cfg[eye].cal, { delta: p[0], alpha: p[1], beta: p[2] });
        if (!r.ok) { this.err(id, r.error === "SPHERE_RANGE" ? "RANGE_SPHERE" : "RANGE_CYL", r.error); return; }
        this.startMove(id, eye, [r.pos.delta, r.pos.alpha, r.pos.beta]); return;
      }
      case "set_pos": {
        if (this.busy(id)) return;
        var c = this.pos[eye];
        this.startMove(id, eye, [m.delta, c[1] + O.wrap180(m.alpha - c[1]), c[2] + O.wrap180(m.beta - c[2])]); return;
      }
      case "calib_get": this.emit({ t: "calib", eye: eye || "R", cal: this.cfg[eye || "R"].cal, fitted: this.cfg[eye || "R"].fitted }); return;
      case "calib_set":
        if (eye !== "R" && eye !== "L") { this.err(id, "BAD_ARG", "need eye"); return; }
        var cal = Object.assign({}, this.cfg[eye].cal, m.cal || {});
        if (!(cal.k > 0.1 && cal.Pp > 0 && cal.Pm < 0)) { this.err(id, "BAD_ARG", "k > 0.1, Pp > 0, Pm < 0"); return; }
        this.cfg[eye] = { cal: cal, fitted: !!m.fitted }; this.conf[eye] = false;
        this.emit({ t: "ack", id: id }); this.emit({ t: "calib", eye: eye, cal: cal, fitted: !!m.fitted }); return;
      case "jog": this.emit({ t: "ack", id: id }); return;
      case "sim_fault":
        if (m.kind === "sensor") this.inject.sensor = true;
        else if (m.kind === "stall") this.inject.stall = true;
        else if (m.kind === "link") { if (this.state === "MOVING" || this.state === "VERIFYING") this.doFault("LINK", "-", "no message from the app during motion"); else this.doFault("LINK", "-", "app link lost"); }
        else if (m.kind === "clear") this.inject.sensor = this.inject.stall = false;
        this.emit({ t: "ack", id: id }); return;
      default: this.err(id, "UNKNOWN", "unknown message type");
    }
  };
  P.step = function (dt) {
    this.now += dt;
    var self = this;
    if (this.state !== "FAULT" && this.inject.sensor) { this.doFault("SENSOR", "R.ringP", "angle sensor not reading (parity, error flag or magnet)"); return; }
    if (this.state === "PARKED") this.setState("READY");
    if (this.state === "MOVING") {
      if (this.linkSeen && this.now - this.lastLink > LINK_TIMEOUT_MS) { this.doFault("LINK", "-", "no message from the app during motion"); return; }
      var e = this.eye, p = this.pos[e], aim = this.aim[e], tgt = this.target[e], done = true;
      for (var j = 0; j < 3; j++) {
        if (this.inject.stall && j === 1) { done = false; continue; }
        var d = aim[j] - p[j], stepMax = SPEED[j] * dt / 1000;
        if (Math.abs(d) <= TOL[j] * 0.5) {
          if (aim[j] !== tgt[j]) { aim[j] = tgt[j]; done = false; } else p[j] = tgt[j];
        } else { p[j] += Math.sign(d) * Math.min(stepMax, Math.abs(d)); done = false; }
      }
      if (this.inject.stall && this.now - this.moveStart > 300) { this.doFault("STALL", e + ".ringP", "drive is not moving toward its target"); return; }
      if (done) { this.verifyStart = this.now; this.vsum = [0, 0, 0]; this.vn = 0; this.setState("VERIFYING"); }
      else if (this.now - this.moveStart > MOVE_TIMEOUT_MS) { this.doFault("TIMEOUT", "-", "move did not finish in time"); return; }
    } else if (this.state === "VERIFYING") {
      var s = this.sensed(this.eye);
      for (var k = 0; k < 3; k++) this.vsum[k] += s[k];
      this.vn++;
      if (this.now - this.verifyStart >= VERIFY_MS) {
        var a = this.vsum.map(function (v) { return v / self.vn; }), t = this.target[this.eye];
        for (var i = 0; i < 3; i++) if (Math.abs(a[i] - t[i]) > VERIFY_TOL[i]) { this.doFault("TOLERANCE", this.eye + "." + AX[i], "outside tolerance after settling"); return; }
        var pos = { delta: a[0], alpha: a[1], beta: a[2] }, cfg = this.cfg[this.eye], pv = O.forward(pos, cfg.cal), rx = O.pvToRx(pv);
        this.conf[this.eye] = true;
        this.setState("CONFIRMED");
        this.emit({ t: "confirmed", id: this.moveId, eye: this.eye, pv: [pv.M, pv.J0, pv.J45], rx: [rx.S, rx.C, rx.ax], pos: a, calibrated: cfg.fitted, model: cfg.cal.model_id });
      }
    }
    var period = (this.state === "MOVING" || this.state === "VERIFYING") ? 50 : 500;
    if (this.now - this.lastStatus >= period) { this.lastStatus = this.now; this.emit(this.status()); }
  };
  // simulation only: power actually delivered (used for the simulated patient's view)
  P.truePower = function (eye) { var p = this.pos[eye]; return O.forward({ delta: p[0], alpha: p[1], beta: p[2] }, this.cfg[eye].cal); };
  return SimDevice;
})();
if (typeof module !== "undefined") module.exports = BasiraSimDevice;
