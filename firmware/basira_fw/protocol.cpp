#include "protocol.h"
#include "config.h"
#include <ArduinoJson.h>
#include <math.h>
#include <string.h>

namespace basira {

static double r4(double x) { return round(x * 1e4) / 1e4; }
static const char* eyeName(int e) { return e == Device::L ? "L" : "R"; }
static int eyeOf(JsonVariantConst v) {
  const char* s = v | "";
  if (s[0] == 'R' && s[1] == 0) return Device::R;
  if (s[0] == 'L' && s[1] == 0) return Device::L;
  return -1;
}

static void emit(ISink* out, JsonDocument& doc) {
  if (!out) return;
  char buf[768];
  size_t n = serializeJson(doc, buf, sizeof(buf));
  if (n >= sizeof(buf) - 1) return;  // never send a truncated line
  out->send(buf);
}

void Protocol::sendInfo() {
  JsonDocument doc;
  doc["t"] = "info";
  doc["fw"] = BASIRA_FW_VERSION;
  doc["proto"] = BASIRA_PROTO_VERSION;
  doc["allow_nominal"] = BASIRA_ALLOW_NOMINAL_CALIB ? true : false;
  for (int e = 0; e < 2; e++) {
    JsonObject o = doc["eyes"][eyeName(e)].to<JsonObject>();
    o["model"] = d_->config(e).cal.model_id;
    o["fitted"] = d_->config(e).fitted;
  }
  emit(out_, doc);
}

void Protocol::sendStatus() {
  JsonDocument doc;
  doc["t"] = "status";
  doc["st"] = stateName(d_->state());
  doc["eye"] = eyeName(d_->activeEye());
  if (d_->state() == State::FAULT) doc["fault"] = faultName(d_->fault());
  for (int e = 0; e < 2; e++) {
    JsonObject o = doc[eyeName(e)].to<JsonObject>();
    Pos p = d_->pos(e);
    JsonArray a = o["pos"].to<JsonArray>();
    a.add(r4(p.delta)); a.add(r4(p.alpha)); a.add(r4(p.beta));
    JsonArray r = o["raw"].to<JsonArray>();
    JsonArray ok = o["ok"].to<JsonArray>();
    for (int j = 0; j < 3; j++) { r.add(r4(d_->raw(e * 3 + j))); ok.add(d_->sensorOk(e * 3 + j) ? 1 : 0); }
    o["occ"] = d_->occluded(e) ? 1 : 0;
    o["conf"] = d_->confirmed(e) ? 1 : 0;
  }
  emit(out_, doc);
}

void Protocol::sendConfig(int e) {
  const EyeConfig& c = d_->config(e);
  JsonDocument doc;
  doc["t"] = "calib";
  doc["eye"] = eyeName(e);
  JsonObject k = doc["cal"].to<JsonObject>();
  k["P0"] = c.cal.P0; k["k"] = c.cal.k; k["Pp"] = c.cal.Pp; k["Pm"] = c.cal.Pm;
  k["J0off"] = c.cal.J0off; k["J45off"] = c.cal.J45off; k["delta_max"] = c.cal.delta_max; k["model_id"] = c.cal.model_id;
  JsonObject m = doc["mech"].to<JsonObject>();
  JsonArray z = m["zero_deg"].to<JsonArray>(), s = m["sign"].to<JsonArray>(), md = m["motor_dir"].to<JsonArray>(), dm = m["dmin"].to<JsonArray>();
  for (int j = 0; j < 3; j++) { z.add(c.zero_deg[j]); s.add(c.sign[j]); md.add(c.motor_dir[j]); dm.add(c.dmin[j]); }
  m["pinion_r_mm"] = c.pinion_r_mm;
  doc["fitted"] = c.fitted;
  emit(out_, doc);
}

void Protocol::handleLine(const char* line) {
  JsonDocument in;
  if (deserializeJson(in, line)) { onError(0, "BAD_JSON", "could not parse message"); return; }
  if (d_) d_->linkActivity();
  const char* t = in["t"] | "";
  int id = in["id"] | 0;
  if (!strcmp(t, "ping")) return;
  if (!strcmp(t, "hello")) { sendInfo(); sendStatus(); return; }
  if (!strcmp(t, "status")) { sendStatus(); return; }
  if (!strcmp(t, "stop")) { d_->cmdStop(id); return; }
  if (!strcmp(t, "reset")) { d_->cmdReset(id); return; }
  int eye = eyeOf(in["eye"]);
  if (!strcmp(t, "set")) {
    if (eye < 0 || !in["S"].is<double>() || !in["C"].is<double>() || !in["ax"].is<double>()) { onError(id, "BAD_ARG", "need eye, S, C, ax"); return; }
    d_->cmdSetRx(id, eye, Rx{in["S"].as<double>(), in["C"].as<double>(), in["ax"].as<double>()});
    return;
  }
  if (!strcmp(t, "set_pos")) {
    if (eye < 0 || !in["delta"].is<double>() || !in["alpha"].is<double>() || !in["beta"].is<double>()) { onError(id, "BAD_ARG", "need eye, delta, alpha, beta"); return; }
    d_->cmdSetPos(id, eye, Pos{in["delta"].as<double>(), in["alpha"].as<double>(), in["beta"].as<double>()});
    return;
  }
  if (!strcmp(t, "occlude")) {
    if (eye < 0) { onError(id, "BAD_ARG", "need eye"); return; }
    d_->cmdOcclude(id, eye, in["on"] | false);
    return;
  }
  if (!strcmp(t, "jog")) {
    d_->cmdJog(id, in["axis"] | -1, in["duty"] | 0.0f, in["ms"] | 0);
    return;
  }
  if (!strcmp(t, "calib_get")) {
    if (eye < 0) { sendConfig(Device::R); sendConfig(Device::L); } else sendConfig(eye);
    return;
  }
  if (!strcmp(t, "calib_set")) {
    if (eye < 0) { onError(id, "BAD_ARG", "need eye"); return; }
    EyeConfig c = d_->config(eye);
    JsonObjectConst k = in["cal"];
    if (!k.isNull()) {
      c.cal.P0 = k["P0"] | c.cal.P0; c.cal.k = k["k"] | c.cal.k; c.cal.Pp = k["Pp"] | c.cal.Pp; c.cal.Pm = k["Pm"] | c.cal.Pm;
      c.cal.J0off = k["J0off"] | c.cal.J0off; c.cal.J45off = k["J45off"] | c.cal.J45off; c.cal.delta_max = k["delta_max"] | c.cal.delta_max;
      const char* mid = k["model_id"] | (const char*)nullptr;
      if (mid) { strncpy(c.cal.model_id, mid, sizeof(c.cal.model_id) - 1); c.cal.model_id[sizeof(c.cal.model_id) - 1] = 0; }
    }
    JsonObjectConst m = in["mech"];
    if (!m.isNull()) {
      for (int j = 0; j < 3; j++) {
        c.zero_deg[j] = m["zero_deg"][j] | c.zero_deg[j];
        c.sign[j] = m["sign"][j] | c.sign[j];
        c.motor_dir[j] = m["motor_dir"][j] | c.motor_dir[j];
        c.dmin[j] = m["dmin"][j] | c.dmin[j];
      }
      c.pinion_r_mm = m["pinion_r_mm"] | c.pinion_r_mm;
    }
    c.fitted = in["fitted"] | c.fitted;
    pendingCfgEye_ = eye; pendingCfg_ = c;
    d_->cmdSetConfig(id, eye, c);   // acknowledges or rejects; saved in onAck
    return;
  }
  onError(id, "UNKNOWN", "unknown message type");
}

void Protocol::onAck(int id) {
  if (pendingCfgEye_ >= 0) {
    if (save_) save_(pendingCfgEye_, pendingCfg_);
    int e = pendingCfgEye_; pendingCfgEye_ = -1;
    JsonDocument doc; doc["t"] = "ack"; doc["id"] = id; emit(out_, doc);
    sendConfig(e);
    return;
  }
  JsonDocument doc; doc["t"] = "ack"; doc["id"] = id; emit(out_, doc);
}

void Protocol::onError(int id, const char* code, const char* msg) {
  pendingCfgEye_ = -1;
  JsonDocument doc; doc["t"] = "err"; doc["id"] = id; doc["code"] = code; doc["msg"] = msg; emit(out_, doc);
}

void Protocol::onState(State, int) { sendStatus(); }

void Protocol::onConfirmed(int id, int eye, const PV& pv, const Rx& rx, const Pos& pos, bool fitted, const char* model) {
  JsonDocument doc;
  doc["t"] = "confirmed"; doc["id"] = id; doc["eye"] = eyeName(eye);
  JsonArray a = doc["pv"].to<JsonArray>(); a.add(r4(pv.M)); a.add(r4(pv.J0)); a.add(r4(pv.J45));
  JsonArray b = doc["rx"].to<JsonArray>(); b.add(r4(rx.S)); b.add(r4(rx.C)); b.add(r4(rx.ax));
  JsonArray c = doc["pos"].to<JsonArray>(); c.add(r4(pos.delta)); c.add(r4(pos.alpha)); c.add(r4(pos.beta));
  doc["calibrated"] = fitted; doc["model"] = model;
  emit(out_, doc);
}

void Protocol::onFault(Fault f, const char* axis, const char* msg) {
  JsonDocument doc; doc["t"] = "fault"; doc["code"] = faultName(f); doc["axis"] = axis; doc["msg"] = msg; emit(out_, doc);
}

}  // namespace basira
