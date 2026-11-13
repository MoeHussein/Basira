// BASIRA optics model, JavaScript port of physics/basira_optics.py (the definition).
// tests/app_test.js checks this port against tests/golden_vectors.json.
var BasiraOptics = (function () {
  "use strict";
  var DEG = Math.PI / 180;

  function nominal() { return { P0: 0, k: 2.0, Pp: 3.0, Pm: -3.0, J0off: 0, J45off: 0, delta_max: 5.0, model_id: "NOMINAL" }; }

  function wrap180(a) { a = a % 180; if (a <= -90) a += 180; if (a > 90) a -= 180; return a; }

  function rxToPv(rx) { // rx = {S, C, ax}, minus cylinder
    return { M: rx.S + rx.C / 2, J0: -(rx.C / 2) * Math.cos(2 * rx.ax * DEG), J45: -(rx.C / 2) * Math.sin(2 * rx.ax * DEG) };
  }
  function pvToRx(pv) {
    var Jm = Math.hypot(pv.J0, pv.J45), C = -2 * Jm, S = pv.M - C / 2;
    if (Jm < 1e-9) return { S: S, C: 0, ax: 180 };
    var ax = ((Math.atan2(pv.J45, pv.J0) / 2 / DEG) % 180 + 180) % 180;
    if (ax < 1e-9) ax = 180;
    return { S: S, C: C, ax: ax };
  }
  function forward(p, c) { // p = {delta, alpha, beta}
    return {
      M: c.P0 + c.k * p.delta + (c.Pp + c.Pm) / 2,
      J0: -(c.Pp / 2) * Math.cos(2 * p.alpha * DEG) - (c.Pm / 2) * Math.cos(2 * p.beta * DEG) + c.J0off,
      J45: -(c.Pp / 2) * Math.sin(2 * p.alpha * DEG) - (c.Pm / 2) * Math.sin(2 * p.beta * DEG) + c.J45off
    };
  }
  // Target power vector -> positions; returns {ok:true, pos} or {ok:false, error:"SPHERE_RANGE"|"CYL_RANGE"}
  function inverse(t, c, cur) {
    var delta = (t.M - c.P0 - (c.Pp + c.Pm) / 2) / c.k;
    if (Math.abs(delta) > c.delta_max + 1e-9) return { ok: false, error: "SPHERE_RANGE" };
    var tx = t.J0 - c.J0off, ty = t.J45 - c.J45off, T = Math.hypot(tx, ty);
    var a_ = c.Pp / 2, b_ = -c.Pm / 2;
    if (T > a_ + b_ + 1e-9) return { ok: false, error: "CYL_RANGE" };
    var cands = [];
    if (T < Math.abs(a_ - b_) + 1e-12 || T < 1e-12) {
      var du;
      if (T < 1e-12) du = 2 * cur.alpha * DEG + Math.PI;
      else { var phi0 = Math.atan2(ty, tx); du = a_ >= b_ ? phi0 : phi0 + Math.PI; }
      cands.push([du, du + Math.PI]);
    } else {
      var phi = Math.atan2(ty, tx);
      var cg = Math.max(-1, Math.min(1, (a_ * a_ + T * T - b_ * b_) / (2 * a_ * T))), g = Math.acos(cg);
      [g, -g].forEach(function (s) {
        var d = phi + s;
        cands.push([d, Math.atan2(ty - a_ * Math.sin(d), tx - a_ * Math.cos(d))]);
      });
    }
    var best = null;
    cands.forEach(function (cd) {
      var a = (cd[0] - Math.PI) / 2 / DEG, b = cd[1] / 2 / DEG;
      a = cur.alpha + wrap180(a - cur.alpha);
      b = cur.beta + wrap180(b - cur.beta);
      var travel = Math.abs(a - cur.alpha) + Math.abs(b - cur.beta);
      if (!best || travel < best.travel - 1e-9) best = { travel: travel, pos: { delta: delta, alpha: a, beta: b } };
    });
    return { ok: true, pos: best.pos };
  }
  function vertexRx(rx, dFrom, dTo) { // per principal meridian: F2 = F1 / (1 - d F1)
    var d = (dFrom - dTo) / 1000, f = function (F) { return F / (1 - d * F); };
    var s2 = f(rx.S), c2 = f(rx.S + rx.C);
    return { S: s2, C: c2 - s2, ax: rx.ax };
  }
  function composeRefinement(rx, jcc, fog) { // jcc: 0, +1, -1 (minus axis at ax +/- 45); fog: plus sphere (D)
    var pv = rxToPv(rx);
    pv.M += fog || 0;
    if (jcc) {
      var a = (rx.ax + (jcc > 0 ? 45 : -45)) * DEG;
      pv.J0 += 0.25 * Math.cos(2 * a);
      pv.J45 += 0.25 * Math.sin(2 * a);
    }
    return pv;
  }
  function q(x) { return Math.round(x * 4) / 4; }
  function fmtRx(rx, round) {
    var S = round ? q(rx.S) : rx.S, C = round ? q(rx.C) : rx.C;
    var sg = function (v) { return (v >= 0 ? "+" : "−") + Math.abs(v).toFixed(2); };
    if ((round && Math.abs(C) < 0.125) || Math.abs(C) < 0.005) return sg(S) + " DS";
    var a = Math.round(rx.ax) % 180; if (a === 0) a = 180;
    return sg(S) + " / " + sg(C) + " × " + a + "°";
  }

  return { nominal: nominal, wrap180: wrap180, rxToPv: rxToPv, pvToRx: pvToRx, forward: forward, inverse: inverse,
           vertexRx: vertexRx, composeRefinement: composeRefinement, fmtRx: fmtRx, DEG: DEG };
})();
if (typeof module !== "undefined") module.exports = BasiraOptics;
