// Host tests for the BASIRA firmware core (optics, axis control, state machine, protocol).
// Build and run: make -C firmware/test_host   (needs only g++; ArduinoJson is vendored in third_party/)
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <ArduinoJson.h>
#include "../basira_fw/config.h"
#include "../basira_fw/device.h"
#include "../basira_fw/optics.h"
#include "../basira_fw/protocol.h"

using namespace basira;
static int fails = 0, checks = 0;
#define CHECK(cond, ...) do { checks++; if (!(cond)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

// ---------------- simulated drive: gearmotor + shaft + AS5048A ----------------
struct SimDrive : IMotor, ISensor {
  double shaft = 0;          // deg, true shaft angle
  double vmax = 600;         // deg/s at full duty
  double deadzone = 0.15;    // duty below which the motor does not move (stiction)
  float duty = 0;
  bool jammed = false, sensorDead = false;
  unsigned seed = 1;
  double noise = 0.03;       // deg, sensor noise (uniform)
  void drive(float d) override { duty = d; }
  bool read(float& deg) override {
    if (sensorDead) return false;
    seed = seed * 1103515245u + 12345u;
    double n = ((int)((seed >> 8) % 2001) - 1000) / 1000.0 * noise;
    double a = fmod(shaft + n, 360.0);
    if (a < 0) a += 360.0;
    deg = (float)a;
    return true;
  }
  void step(double dt) {
    if (jammed || fabs(duty) <= deadzone) return;
    double eff = (fabs(duty) - deadzone) / (1 - deadzone);
    shaft += (duty > 0 ? 1 : -1) * eff * vmax * dt;
  }
};

struct SimBoard : IBoard {
  bool enabled = false, drvFault = false, servo_[2] = {false, false};
  void motorsEnable(bool on) override { enabled = on; }
  bool driverFault() override { return drvFault; }
  void servo(int eye, bool closed) override { servo_[eye] = closed; }
};

struct Capture : ISink {
  std::vector<std::string> lines;
  void send(const char* json) override { lines.push_back(json); }
  int count(const char* type) {
    int n = 0;
    std::string key = std::string("\"t\":\"") + type + "\"";
    for (auto& l : lines) if (l.find(key) != std::string::npos) n++;
    return n;
  }
  std::string last(const char* type) {
    std::string key = std::string("\"t\":\"") + type + "\"";
    for (auto it = lines.rbegin(); it != lines.rend(); ++it) if (it->find(key) != std::string::npos) return *it;
    return "";
  }
};

static int saves = 0;
static void saver(int, const EyeConfig&) { saves++; }

struct Rig {
  SimDrive drv[6];
  SimBoard board;
  Device dev;
  Protocol proto;
  Capture out;
  uint32_t t = 0;
  Rig() {
    IMotor* m[6]; ISensor* s[6];
    for (int i = 0; i < 6; i++) {
      drv[i].seed = 17 + i;
      if (i % 3 == 0) { drv[i].vmax = 120; }            // plate pinion: 120 deg/s = 10.5 mm/s at r = 5 mm
      else { drv[i].vmax = 150; drv[i].shaft = 45; }    // rings start at 45 deg
      m[i] = &drv[i]; s[i] = &drv[i];
    }
    EyeConfig c;
    dev.begin(m, s, &board, &proto, c, c);
    proto.begin(&dev, &out, saver);
  }
  void run(uint32_t ms, bool keepAlive = true) {
    for (uint32_t k = 0; k < ms; k += 2) {
      for (auto& d : drv) d.step(0.002);
      t += 2;
      if (keepAlive && t % 400 == 0) proto.handleLine("{\"t\":\"ping\"}");
      dev.tick(t);
    }
  }
  bool runUntil(State s, uint32_t maxMs) {
    for (uint32_t k = 0; k < maxMs; k += 2) { run(2); if (dev.state() == s) return true; }
    return false;
  }
  void send(const std::string& j) { proto.handleLine(j.c_str()); }
};

static std::string setMsg(int id, const char* eye, double S, double C, double ax) {
  char b[160];
  snprintf(b, sizeof b, "{\"t\":\"set\",\"id\":%d,\"eye\":\"%s\",\"S\":%.4f,\"C\":%.4f,\"ax\":%.4f}", id, eye, S, C, ax);
  return b;
}

// ---------------- tests ----------------
static void test_golden(const char* path) {
  std::ifstream f(path);
  CHECK(f.good(), "cannot open %s", path);
  if (!f.good()) return;
  std::stringstream ss; ss << f.rdbuf();
  JsonDocument doc;
  CHECK(!deserializeJson(doc, ss.str()), "golden json parse");
  std::vector<Calib> cals;
  for (JsonObject c : doc["cals"].as<JsonArray>()) {
    Calib k; k.P0 = c["P0"]; k.k = c["k"]; k.Pp = c["Pp"]; k.Pm = c["Pm"]; k.J0off = c["J0off"]; k.J45off = c["J45off"]; k.delta_max = c["delta_max"];
    cals.push_back(k);
  }
  int n = 0; double worst = 0, worstPos = 0;
  for (JsonObject c : doc["inverse_cases"].as<JsonArray>()) {
    const Calib& k = cals[c["cal"].as<int>()];
    Rx rx{c["rx"][0], c["rx"][1], c["rx"][2]};
    Pos cur{c["cur"][0], c["cur"][1], c["cur"][2]}, out{};
    InvResult r = inverse(rxToPv(rx), k, cur, out);
    if (c["error"].is<const char*>()) {
      const char* e = c["error"];
      CHECK((!strcmp(e, "SPHERE_RANGE") && r == InvResult::SPHERE_RANGE) || (!strcmp(e, "CYL_RANGE") && r == InvResult::CYL_RANGE), "expected %s", e);
    } else {
      CHECK(r == InvResult::OK, "inverse failed");
      double d[3] = {out.delta - c["pos"][0].as<double>(), out.alpha - c["pos"][1].as<double>(), out.beta - c["pos"][2].as<double>()};
      PV pv = forward(out, k);
      double e[3] = {pv.M - c["pv"][0].as<double>(), pv.J0 - c["pv"][1].as<double>(), pv.J45 - c["pv"][2].as<double>()};
      for (double v : d) worstPos = fmax(worstPos, fabs(v));
      for (double v : e) worst = fmax(worst, fabs(v));
    }
    n++;
  }
  // ArduinoJson keeps ~9 significant digits when parsing. Delivered power must agree to 1e-6 D. Positions are
  // checked to 1e-3 deg/mm: near zero cylinder with a mismatched pair the ring solution is ill-conditioned
  // (acos near +/-1), so 1e-9 input differences move the angles by ~1e-4 deg with no change in power.
  CHECK(worst < 1e-6, "golden power mismatch %.3e D", worst);
  CHECK(worstPos < 1e-3, "golden position mismatch %.3e", worstPos);
  double wc = 0;
  for (JsonObject c : doc["conversions"].as<JsonArray>()) {
    PV pv = rxToPv(Rx{c["rx"][0], c["rx"][1], c["rx"][2]});
    Rx back = pvToRx(pv);
    wc = fmax(wc, fabs(pv.M - c["pv"][0].as<double>()) + fabs(pv.J0 - c["pv"][1].as<double>()) + fabs(pv.J45 - c["pv"][2].as<double>()));
    wc = fmax(wc, fabs(back.S - c["rx_back"][0].as<double>()) + fabs(back.C - c["rx_back"][1].as<double>()) + fabs(back.ax - c["rx_back"][2].as<double>()));
  }
  CHECK(wc < 1e-5, "golden conversion mismatch %.3e", wc);
  printf("golden vectors: %d inverse cases, max power error %.1e D, max position error %.1e\n", n, worst, worstPos);
}

static void test_exam_flow() {
  Rig r;
  r.run(10);
  CHECK(r.dev.state() == State::READY, "boot -> READY, got %s", stateName(r.dev.state()));
  r.send("{\"t\":\"hello\"}");
  CHECK(r.out.count("info") >= 1, "hello -> info");
  r.send(setMsg(1, "R", -2.25, -0.75, 175));
  CHECK(r.out.last("ack").find("\"id\":1") != std::string::npos, "set acknowledged");
  CHECK(r.dev.state() == State::MOVING, "MOVING after set");
  r.send(setMsg(2, "R", -1, 0, 180));
  CHECK(r.out.last("err").find("BUSY") != std::string::npos, "second set while moving -> BUSY");
  CHECK(r.runUntil(State::CONFIRMED, 4000), "reaches CONFIRMED, state %s fault %s", stateName(r.dev.state()), faultName(r.dev.fault()));
  std::string c = r.out.last("confirmed");
  JsonDocument d; deserializeJson(d, c);
  double S = d["rx"][0], C = d["rx"][1], ax = d["rx"][2];
  CHECK(fabs(S + 2.25) < 0.06 && fabs(C + 0.75) < 0.06 && fabs(wrap180(ax - 175)) < 2.0, "confirmed %s", c.c_str());
  CHECK(d["calibrated"] == false && !strcmp(d["model"] | "", "NOMINAL"), "nominal model flagged: %s", c.c_str());
  printf("exam flow: confirmed %.2f / %.2f x %.0f after %.2f s\n", S, C, ax, r.t / 1000.0);
  // ranges
  r.send(setMsg(3, "R", -12, 0, 180));
  CHECK(r.out.last("err").find("RANGE_SPHERE") != std::string::npos, "sphere range");
  r.send(setMsg(4, "R", 0, -6.5, 90));
  CHECK(r.out.last("err").find("RANGE_CYL") != std::string::npos, "cylinder range");
  r.send(setMsg(5, "R", 0, 0.5, 90));
  CHECK(r.out.last("err").find("BAD_RX") != std::string::npos, "plus cylinder rejected");
  // left eye, occluder
  r.send("{\"t\":\"occlude\",\"id\":6,\"eye\":\"L\",\"on\":true}");
  CHECK(r.board.servo_[1] && r.dev.occluded(Device::L), "occluder closed");
  r.send(setMsg(7, "L", 1.5, -1.25, 90));
  CHECK(r.runUntil(State::CONFIRMED, 4000), "left eye confirmed");
  CHECK(r.dev.confirmed(Device::R) && r.dev.confirmed(Device::L), "both eyes stay confirmed");
}

static void test_backlash_approach() {
  Rig r; r.run(10);
  r.send(setMsg(1, "R", 0, -2, 90));
  r.runUntil(State::CONFIRMED, 4000);
  double startAlpha = r.dev.pos(Device::R).alpha;
  r.send(setMsg(2, "R", 0, -2, 70));   // rings move down by 20 deg
  double minA = 1e9;
  for (int k = 0; k < 2000 && r.dev.state() != State::CONFIRMED; k++) { r.run(2); minA = fmin(minA, r.dev.pos(Device::R).alpha); }
  double tgt = r.dev.target(Device::R).alpha;
  CHECK(r.dev.state() == State::CONFIRMED, "confirmed after downward move");
  CHECK(minA < tgt - 1.0 && tgt < startAlpha, "final approach from below: min %.2f target %.2f", minA, tgt);
}

static void test_faults() {
  { // sensor dies mid-move
    Rig r; r.run(10);
    r.send(setMsg(1, "R", -4, -2, 30));
    r.run(100);
    r.drv[1].sensorDead = true;
    r.run(50);
    CHECK(r.dev.state() == State::FAULT && r.dev.fault() == Fault::SENSOR, "sensor fault, got %s", faultName(r.dev.fault()));
    CHECK(r.out.last("fault").find("R.ringP") != std::string::npos, "fault names the axis: %s", r.out.last("fault").c_str());
    CHECK(!r.dev.confirmed(Device::R), "correction unknown");
    r.send(setMsg(2, "R", -1, 0, 180));
    CHECK(r.out.last("err").find("\"FAULT\"") != std::string::npos, "commands refused in FAULT");
    r.send("{\"t\":\"reset\",\"id\":3}");
    CHECK(r.dev.state() == State::FAULT && r.out.last("err").find("SENSOR") != std::string::npos, "reset refused while sensor is dead");
    r.drv[1].sensorDead = false;
    r.send("{\"t\":\"reset\",\"id\":4}");
    CHECK(r.dev.state() == State::READY, "reset -> READY once the sensor reads");
  }
  { // jammed drive -> STALL
    Rig r; r.run(10);
    r.drv[0].jammed = true;
    r.send(setMsg(1, "R", -4, 0, 180));
    r.run(1000);
    CHECK(r.dev.state() == State::FAULT && r.dev.fault() == Fault::STALL, "stall fault, got %s / %s", stateName(r.dev.state()), faultName(r.dev.fault()));
  }
  { // motor below its dead zone everywhere: dmin set too low -> STALL, not a silent hang
    Rig r; r.run(10);
    for (auto& d : r.drv) d.deadzone = 0.5;
    r.send(setMsg(1, "R", -2, 0, 180));
    r.run(1500);
    CHECK(r.dev.state() == State::FAULT && r.dev.fault() == Fault::STALL, "dead-zone stall, got %s", faultName(r.dev.fault()));
  }
  { // app goes silent during a slow move -> LINK
    Rig r; r.run(10);
    for (auto& d : r.drv) d.vmax = 20;
    r.send(setMsg(1, "R", -8, 0, 180));
    r.run(2000, false);
    CHECK(r.dev.state() == State::FAULT && r.dev.fault() == Fault::LINK, "link watchdog, got %s / %s", stateName(r.dev.state()), faultName(r.dev.fault()));
  }
  { // operator stop
    Rig r; r.run(10);
    r.send(setMsg(1, "R", -4, -2, 30));
    r.run(50);
    r.send("{\"t\":\"stop\",\"id\":2}");
    CHECK(r.dev.state() == State::FAULT && r.dev.fault() == Fault::USER_STOP, "stop");
    for (auto& d : r.drv) CHECK(d.duty == 0, "motors braked after stop");
    const double stopped = r.drv[1].shaft;
    r.send(R"({"t":"jog","id":3,"axis":1,"duty":0.5,"ms":100})");
    CHECK(r.out.last("err").find(R"("id":3,"code":"FAULT")") != std::string::npos, "jog refused after STOP");
    r.run(100);
    CHECK(r.drv[1].shaft == stopped && r.drv[1].duty == 0, "shaft stays still after refused jog");
    r.send(R"({"t":"occlude","id":4,"eye":"R","on":true})");
    CHECK(r.out.last("err").find(R"("id":4,"code":"FAULT")") != std::string::npos, "occluder refused after STOP");
    CHECK(!r.board.servo_[0] && !r.dev.occluded(Device::R), "occluder stays in place after refused command");
    r.send(R"({"t":"reset","id":5})");
    r.send(R"({"t":"occlude","id":6,"eye":"R","on":true})");
    CHECK(r.board.servo_[0] && r.dev.occluded(Device::R), "occluder can operate after reset");
    r.send(R"({"t":"jog","id":7,"axis":1,"duty":0.5,"ms":100})");
    r.run(10);
    CHECK(r.drv[1].shaft > stopped && r.drv[1].duty > 0, "jog can operate after reset");
    r.send(R"({"t":"jog","id":8,"axis":2,"duty":0.5,"ms":100})");
    CHECK(r.out.last("err").find(R"("id":8,"code":"BUSY")") != std::string::npos, "second jog refused during an active jog");
    r.send(R"({"t":"stop","id":9})");
    r.run(110);
    for (auto& d : r.drv) CHECK(d.duty == 0, "STOP cancels an active jog");
  }
  { // something pushes a ring after confirmation -> DRIFT
    Rig r; r.run(10);
    r.send(setMsg(1, "R", -2, -1, 90));
    r.runUntil(State::CONFIRMED, 4000);
    r.drv[2].shaft += 3.0;
    r.run(20);
    CHECK(r.dev.state() == State::FAULT && r.dev.fault() == Fault::DRIFT, "drift fault, got %s", faultName(r.dev.fault()));
  }
  { // driver nFAULT
    Rig r; r.run(10);
    r.board.drvFault = true;
    r.run(4);
    CHECK(r.dev.state() == State::FAULT && r.dev.fault() == Fault::DRIVER, "driver fault");
  }
  printf("faults: sensor, stall, dead zone, link, stop, drift, driver\n");
}

static void test_protocol_misc() {
  Rig r; r.run(10);
  r.send("not json");
  CHECK(r.out.last("err").find("BAD_JSON") != std::string::npos, "bad json");
  r.send("{\"t\":\"warp\"}");
  CHECK(r.out.last("err").find("UNKNOWN") != std::string::npos, "unknown type");
  r.send("{\"t\":\"set\",\"id\":9,\"eye\":\"X\",\"S\":0,\"C\":0,\"ax\":90}");
  CHECK(r.out.last("err").find("BAD_ARG") != std::string::npos, "bad eye");
  saves = 0;
  r.send("{\"t\":\"calib_set\",\"id\":10,\"eye\":\"R\",\"cal\":{\"P0\":0.1,\"k\":1.95,\"Pp\":3.02,\"Pm\":-2.98,\"model_id\":\"FIT-2026-11-R1\"},\"mech\":{\"zero_deg\":[12.5,200,33]},\"fitted\":true}");
  CHECK(saves == 1, "calib_set saved");
  CHECK(r.dev.config(Device::R).fitted && fabs(r.dev.config(Device::R).cal.k - 1.95) < 1e-6, "calib applied");
  CHECK(r.out.last("calib").find("FIT-2026-11-R1") != std::string::npos, "calib echoed");
  r.send("{\"t\":\"calib_set\",\"id\":11,\"eye\":\"R\",\"cal\":{\"k\":0}}");
  CHECK(r.out.last("err").find("BAD_ARG") != std::string::npos && saves == 1, "invalid calibration rejected, not saved");
  r.send("{\"t\":\"status\"}");
  std::string st = r.out.last("status");
  CHECK(st.find("\"st\":\"READY\"") != std::string::npos, "status: %s", st.c_str());
  printf("status line: %zu bytes (budget at 460800 baud, 20 Hz: %d)\n", st.size(), 460800 / 10 / 20);
  CHECK(st.size() < 460800 / 10 / 20 / 2, "status fits the serial budget with margin");
}

int main(int argc, char** argv) {
  test_golden(argc > 1 ? argv[1] : "../../tests/golden_vectors.json");
  test_exam_flow();
  test_backlash_approach();
  test_faults();
  test_protocol_misc();
  printf("%d checks, %d failed\n", checks, fails);
  return fails ? 1 : 0;
}
