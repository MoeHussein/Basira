// BASIRA protocol v1: one JSON object per line, same messages over USB serial and Wi-Fi WebSocket.
// Specification: docs/PROTOCOL.md (the app implements the other side; test_host checks this side).
#pragma once
#include "device.h"

namespace basira {

struct ISink {                                  // broadcast one JSON line to every connected client
  virtual void send(const char* json) = 0;
  virtual ~ISink() {}
};

class Protocol : public IListener {
 public:
  typedef void (*ConfigSaver)(int eye, const EyeConfig& c);
  void begin(Device* dev, ISink* out, ConfigSaver saver) { d_ = dev; out_ = out; save_ = saver; }
  void handleLine(const char* line);            // one received message
  void sendStatus();
  void sendInfo();
  void sendConfig(int eye);

  // IListener
  void onAck(int id) override;
  void onError(int id, const char* code, const char* msg) override;
  void onState(State s, int eye) override;
  void onConfirmed(int id, int eye, const PV& pv, const Rx& rx, const Pos& pos, bool fitted, const char* model) override;
  void onFault(Fault f, const char* axis, const char* msg) override;

 private:
  Device* d_ = nullptr;
  ISink* out_ = nullptr;
  ConfigSaver save_ = nullptr;
  int pendingCfgEye_ = -1;
  EyeConfig pendingCfg_;
};

}  // namespace basira
