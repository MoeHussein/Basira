// Links between the app and a device. All three expose: connect() -> Promise, send(obj), close(),
// and call onmessage(obj) / onclose(reason). Messages are protocol v1 JSON objects (docs/PROTOCOL.md).
var BasiraTransports = (function () {
  "use strict";

  // In-browser simulated device
  function SimTransport() {
    this.kind = "sim"; this.dev = null; this.onmessage = null; this.onclose = null;
  }
  SimTransport.prototype.connect = function () {
    var self = this;
    this.dev = new BasiraSimDevice();
    this.dev.onLine = function (line) { if (self.onmessage) self.onmessage(JSON.parse(line)); };
    this.dev.start();
    return Promise.resolve();
  };
  SimTransport.prototype.send = function (obj) { if (this.dev) this.dev.receive(JSON.stringify(obj)); };
  SimTransport.prototype.close = function () { if (this.dev) this.dev.stop(); this.dev = null; if (this.onclose) this.onclose("closed"); };

  // USB cable: Web Serial (Chrome / Edge on desktop and Android; not Safari / iPad)
  function SerialTransport(baud) {
    this.kind = "usb"; this.baud = baud || 460800; this.port = null; this.writer = null; this.reading = false;
    this.onmessage = null; this.onclose = null;
  }
  SerialTransport.supported = function () { return typeof navigator !== "undefined" && "serial" in navigator; };
  SerialTransport.prototype.connect = async function () {
    if (!SerialTransport.supported()) throw new Error("هذا المتصفح لا يدعم الاتصال عبر USB (استخدم Chrome أو Edge)");
    this.port = await navigator.serial.requestPort();
    await this.port.open({ baudRate: this.baud });
    this.writer = this.port.writable.getWriter();
    this.readLoop();
  };
  SerialTransport.prototype.readLoop = async function () {
    var self = this, dec = new TextDecoder(), buf = "";
    this.reading = true;
    var reader = this.port.readable.getReader();
    this.reader = reader;
    try {
      for (;;) {
        var r = await reader.read();
        if (r.done) break;
        buf += dec.decode(r.value, { stream: true });
        var i;
        while ((i = buf.indexOf("\n")) >= 0) {
          var line = buf.slice(0, i).trim(); buf = buf.slice(i + 1);
          if (line.charAt(0) === "{") { try { self.onmessage && self.onmessage(JSON.parse(line)); } catch (e) { /* not protocol output */ } }
        }
      }
    } catch (e) { /* device unplugged */ }
    this.reading = false;
    if (this.onclose) this.onclose("serial closed");
  };
  SerialTransport.prototype.send = function (obj) {
    if (this.writer) this.writer.write(new TextEncoder().encode(JSON.stringify(obj) + "\n"));
  };
  SerialTransport.prototype.close = async function () {
    try { if (this.reader) await this.reader.cancel(); } catch (e) {}
    try { if (this.writer) this.writer.releaseLock(); } catch (e) {}
    try { if (this.port) await this.port.close(); } catch (e) {}
  };

  // Wi-Fi: the device is an access point and serves this app; WebSocket at /ws
  function WsTransport(url) {
    this.kind = "wifi"; this.url = url; this.ws = null; this.onmessage = null; this.onclose = null;
  }
  WsTransport.defaultUrl = function () {
    var h = (typeof location !== "undefined" && location.protocol === "http:" && location.host) ? location.host : "192.168.4.1";
    return "ws://" + h + "/ws";
  };
  WsTransport.prototype.connect = function () {
    var self = this;
    return new Promise(function (resolve, reject) {
      var ws = new WebSocket(self.url);
      self.ws = ws;
      var opened = false;
      ws.onopen = function () { opened = true; resolve(); };
      ws.onerror = function () { if (!opened) reject(new Error("تعذّر الاتصال بالجهاز عبر Wi-Fi: " + self.url)); };
      ws.onmessage = function (ev) { try { self.onmessage && self.onmessage(JSON.parse(ev.data)); } catch (e) {} };
      ws.onclose = function () { if (opened && self.onclose) self.onclose("wifi closed"); };
    });
  };
  WsTransport.prototype.send = function (obj) { if (this.ws && this.ws.readyState === 1) this.ws.send(JSON.stringify(obj)); };
  WsTransport.prototype.close = function () { if (this.ws) this.ws.close(); };

  return { SimTransport: SimTransport, SerialTransport: SerialTransport, WsTransport: WsTransport };
})();
if (typeof module !== "undefined") module.exports = BasiraTransports;
