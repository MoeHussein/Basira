// App-side tests: JS optics port vs golden vectors, and the simulated device's protocol behaviour.
// Run: node tests/app_test.js
"use strict";
const path = require("path"), fs = require("fs");
const O = require(path.join(__dirname, "..", "app", "js", "optics.js"));
global.BasiraOptics = O;
const SimDevice = require(path.join(__dirname, "..", "app", "js", "sim_device.js"));
let checks = 0, fails = 0;
function check(c, msg) { checks++; if (!c) { fails++; console.log("FAIL", msg); } }

// 1. golden vectors
const g = JSON.parse(fs.readFileSync(path.join(__dirname, "golden_vectors.json"), "utf8"));
let worstPv = 0, worstPos = 0;
for (const c of g.inverse_cases) {
  const cal = g.cals[c.cal];
  const r = O.inverse(O.rxToPv({ S: c.rx[0], C: c.rx[1], ax: c.rx[2] }), cal, { delta: c.cur[0], alpha: c.cur[1], beta: c.cur[2] });
  if (c.error) { check(!r.ok && r.error === c.error, "expected " + c.error + " got " + JSON.stringify(r)); continue; }
  check(r.ok, "inverse failed " + JSON.stringify(c.rx));
  const pv = O.forward(r.pos, cal);
  worstPv = Math.max(worstPv, Math.abs(pv.M - c.pv[0]), Math.abs(pv.J0 - c.pv[1]), Math.abs(pv.J45 - c.pv[2]));
  worstPos = Math.max(worstPos, Math.abs(r.pos.delta - c.pos[0]), Math.abs(r.pos.alpha - c.pos[1]), Math.abs(r.pos.beta - c.pos[2]));
}
check(worstPv < 1e-9, "golden power " + worstPv);
check(worstPos < 1e-6, "golden positions " + worstPos);
let worstConv = 0;
const d3 = (a, b) => Math.max(...a.map((v, i) => Math.abs(v - b[i])));
for (const c of g.conversions) {
  const rx = { S: c.rx[0], C: c.rx[1], ax: c.rx[2] }, pv = O.rxToPv(rx), back = O.pvToRx(pv);
  const v12 = O.vertexRx(rx, 13, 12), v0 = O.vertexRx(rx, 13, 0);
  const jp = O.composeRefinement(rx, 1, 0), jm = O.composeRefinement(rx, -1, 0), fg = O.composeRefinement(rx, 0, 1);
  worstConv = Math.max(worstConv, d3([pv.M, pv.J0, pv.J45], c.pv), d3([back.S, back.C, back.ax], c.rx_back), d3([v12.S, v12.C, v12.ax], c.vertex_13_12),
    d3([v0.S, v0.C, v0.ax], c.vertex_13_0), d3([jp.M, jp.J0, jp.J45], c.jcc_plus), d3([jm.M, jm.J0, jm.J45], c.jcc_minus), d3([fg.M, fg.J0, fg.J45], c.fog));
}
check(worstConv < 1e-9, "golden conversions " + worstConv);
console.log(`golden: ${g.inverse_cases.length} inverse cases (power ${worstPv.toExponential(1)} D, positions ${worstPos.toExponential(1)}), ${g.conversions.length} conversions (${worstConv.toExponential(1)})`);

// 2. simulated device: protocol behaviour
function rig() {
  const d = new SimDevice({ seed: 7 }), out = [];
  d.onLine = l => out.push(JSON.parse(l));
  const run = ms => { for (let t = 0; t < ms; t += 10) { if (t % 400 === 0) d.receive('{"t":"ping"}'); d.step(10); } };
  const last = type => [...out].reverse().find(m => m.t === type);
  return { d, out, run, last, send: o => d.receive(JSON.stringify(o)) };
}
{
  const r = rig();
  r.run(20);
  r.send({ t: "hello" });
  check(r.last("info") && r.last("info").proto === 1, "info");
  check(r.last("status").st === "READY", "READY");
  r.send({ t: "set", id: 1, eye: "R", S: -2.25, C: -0.75, ax: 175 });
  check(r.last("ack").id === 1 && r.d.state === "MOVING", "ack + MOVING");
  r.send({ t: "set", id: 2, eye: "R", S: 0, C: 0, ax: 180 });
  check(r.last("err").code === "BUSY", "BUSY");
  r.run(3000);
  const c = r.last("confirmed");
  check(c && c.id === 1 && Math.abs(c.rx[0] + 2.25) < 0.06 && Math.abs(c.rx[1] + 0.75) < 0.06 && Math.abs(O.wrap180(c.rx[2] - 175)) < 2, "confirmed " + JSON.stringify(c));
  check(c && c.calibrated === false && c.model === "NOMINAL", "nominal flagged");
  r.send({ t: "set", id: 3, eye: "R", S: -12, C: 0, ax: 180 });
  check(r.last("err").code === "RANGE_SPHERE", "sphere range");
  r.send({ t: "set", id: 4, eye: "R", S: 0, C: -6.5, ax: 90 });
  check(r.last("err").code === "RANGE_CYL", "cyl range");
  r.send({ t: "sim_fault", id: 5, kind: "sensor" });
  r.run(20);
  check(r.d.state === "FAULT" && r.last("fault").code === "SENSOR", "sensor fault");
  r.send({ t: "reset", id: 6 });
  check(r.last("err").code === "SENSOR", "reset refused while sensor dead");
  r.send({ t: "sim_fault", id: 7, kind: "clear" });
  r.send({ t: "reset", id: 8 });
  check(r.d.state === "READY", "reset after repair");
  r.send({ t: "sim_fault", id: 9, kind: "stall" });
  r.send({ t: "set", id: 10, eye: "L", S: 1, C: -1, ax: 90 });
  r.run(600);
  check(r.d.state === "FAULT" && r.last("fault").code === "STALL", "stall fault");
  r.send({ t: "reset", id: 11 });
  r.send({ t: "set", id: 12, eye: "L", S: 9, C: 0, ax: 180 });
  r.run(3000);
  check(r.d.state === "CONFIRMED", "full-range move confirmed");
  r.send({ t: "set", id: 13, eye: "L", S: -9.5, C: 0, ax: 180 });   // ~1.6 s of travel
  for (let t = 0; t < 2000; t += 10) r.d.step(10);          // app silent during motion
  check(r.d.state === "FAULT" && r.last("fault").code === "LINK", "link watchdog, got " + r.d.state);
  console.log("simulated device: exam, ranges, sensor, stall, link watchdog");
}
console.log(`${checks} checks, ${fails} failed`);
process.exit(fails ? 1 : 0);
