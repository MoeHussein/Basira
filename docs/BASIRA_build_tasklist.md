# BASIRA — build task list (functional prototype)

This list follows the rev 9 blueprint: an Alvarez sphere plus a ±3.00 D cylinder pair, controlled from an app.

**Ground rules:**
- **CAD tool:** OpenSCAD. Every `.scad` file keeps the numbers you measure at the top.
- **Workflow:** measure the real part, type the number in, export an STL, then print.
- **Order:** prove the optics, then motorize one eye, then build the second eye and the headset.
- **Measure first:** never print a mount before the part it holds is in your hand.

---

## Stage 1 — prove the optics, no motors (gates G1 and G2)

**Borrow:**
- [ ] A trial lens set that includes the +3.00 DC and −3.00 DC cylinders.
- [ ] A lensmeter, plus 1–2 hours of an optometrist's time.

**Buy:**
- [ ] 1 pair of adjustable-focus glasses that use **two sliding plates per eye** (Alvarez type). Check before buying; some adjustable glasses are fluid-filled.
- [ ] A digital caliper reading to 0.01 mm, if the lab has none.
- [ ] An M2/M3 screw, nut and heat-set insert kit.

**Print:**
- [ ] `cad/basira_g1_stokes_fixture.scad` (ready now): **base, ring 1, ring 2**.
  1. Measure the five lens numbers at the top of the file first: rim diameter, rim thickness, handle width, handle length, glass bulge.
  2. Set `part`, render with F6, then export the STL. Repeat for each part.
  3. Print in PLA or PETG: 0.2 mm layers, 4 walls, 30 % infill, scale face up, no supports.
  4. Before printing, check that a Ø148 mm disc fits on the lensmeter around the lens stop.
- [ ] `cad/basira_g2_alvarez_jig.scad`: I write this once the plates are out and measured (width, height, thickness, how they are guided). It holds one plate fixed and moves the other on an M3 screw, at 0.5 mm per turn.

**Measure (record everything in a CSV):**
- [ ] **G1:**
  1. Put the −3.00 DC lens in ring 1 and the +3.00 DC lens in ring 2.
  2. Set ring 1 (β) to 0° and ring 2 (Δ) to 0, 10, 20, 30, 45, 60 and 90°. Read SPH, CYL and AX on the lensmeter.
  3. Repeat at β = 45°.
  4. The first readings also fix the sign convention and the zero offset of each lens's axis mark.
- [ ] **G2:**
  1. Read the plates' power at 7 slide positions, 3 times each.
  2. Push one plate sideways and record how much cylinder that adds.
- [ ] **Decision:**
  - G2 linear with a useful range → Alvarez sphere.
  - Otherwise → trial-lens sphere wheel.

**Program (Python, one package `basira/`):**
- [ ] **P1** `basira/optics.py`: Stokes control law, Alvarez law, vertex conversion and power vectors. Lift this from `verify.py` and add unit tests.
- [ ] **P1b** `tools/g1_compare.py`: reads the G1 CSV and prints measured minus predicted M, J0 and J45.

---

## Stage 2 — one motorized eye (gate G3)

**Buy (one eye, plus spares):**
- [ ] ESP32-S3-DevKitC-1 × 1
- [ ] DRV8833 dual motor-driver breakout × 2 (3 motors)
- [ ] Micro metal gearmotors, 6 V, × 4. Get two ratios (≈100:1 and ≈300:1) for the torque test.
- [ ] AS5048A breakout boards × 3, plus diametric magnets Ø6–8 × 2.5–3 mm × 5
- [ ] Thin-section ball bearings × 2 for the cylinder rings, e.g. 61806 (30 × 42 × 7 mm; check before ordering)
- [ ] Module-0.5 brass racks × 2 and pinions × 3, for the sphere slide and the two ring drives
- [ ] Sphere-slide guides: Ø3 mm stainless dowel pins × 4 with brass bushings, or 1 MGN5 miniature rail
- [ ] Micro servo, 9 g class, × 1 (occluder)
- [ ] Own pair of ±3.00 DC trial lenses × 1. An optical lab edges them round to Ø26 mm; CR-39 is easier to edge than glass.
- [ ] Wiring: JST-XH connectors, silicone wire, a USB-C cable. Bench supply: a lab current-limited supply.

**Print (I write the CAD after Stage 1, from the parts in hand):**
- [ ] One-eye optical module frame: sphere slide on the guides, two bearing seats, three motor mounts, three sensor mounts, occluder hinge.
- [ ] Cylinder ring carriers × 2: hold the Ø26 lenses, gear teeth on the rim.
  - Print the gears and rings in SLA resin.
  - Print the frame in PETG.
  - Install the BOSL2 library in OpenSCAD on your Mac for the gear and rack shapes.
- [ ] Occluder flap.

**Program:**
- [ ] **P2 Firmware v0** (ESP32-S3, PlatformIO + Arduino):
  - read the AS5048A sensors in an SPI chain;
  - hold each motor at a target angle (PID), with timeout and stall detection;
  - home the sphere slide against its end stop;
  - drive the occluder servo.
- [ ] **P2b Firmware states:** PARKED → READY → MOVING → VERIFYING → CONFIRMED / FAULT. Firmware owns the mechanical truth (angles, faults); it does no optics maths.
- [ ] **P3 Serial protocol:** JSON lines over USB.
  - Command: `{"id":7,"cmd":"move","eye":"R","sph_mm":…,"ring1_deg":…,"ring2_deg":…}`
  - Reply: `{"id":7,"state":"CONFIRMED","sensors":{…}}`
- [ ] **P4 Bench app v0** (Python, laptop):
  - choose the eye and enter SPH/CYL/AX;
  - `optics.py` plus the calibration model turn those into mechanical targets, which are sent to the firmware;
  - show requested and confirmed values;
  - log to CSV.
- [ ] **P5 Calibration tool:**
  1. Run a guided lensmeter session of 30–60 settings.
  2. Fit the model.
  3. Save the model as JSON with an ID.
  4. Run a held-out check on 20 random settings.

**Test:**
- [ ] Faults: unplug a sensor, stall a motor, unplug USB, force a timeout. Each must end in FAULT with the correction marked unknown.
- [ ] **G3 pass:** 20 random settings agree with the lensmeter within the optometrist's limits.

---

## Stage 3 — two eyes and the headset (gates G4 and G5)

- [ ] Buy and print a second optical module (repeat Stage 2).
- [ ] Print a full-size **dummy headset**: check fit, nose clearance and PD range; weigh it and find the centre of gravity.
- [ ] Print:
  - the bridge with PD slide and lock;
  - the BVD side scale;
  - covers;
  - the strap buckle.
- [ ] Buy the strap and face-gasket material.
- [ ] **P6 App v1:**
  - the exam screen from the canvas design;
  - both eyes;
  - JCC flips, fog, chart control on a second screen;
  - the three outputs: device plane, spectacle Rx, corneal plane.
  - Choose the tablet stack here, and port `optics.py` using golden test vectors so the maths stays identical.
- [ ] **G5 headform tests:**
  - both eyes;
  - occlusion;
  - BVD measurement;
  - JCC flip time;
  - quick release with the power off.

## Stage 4 — product route (G6, not a build task)

- [ ] Custom moulded Alvarez plates.
- [ ] Patent (freedom-to-operate) search.
- [ ] SFDA / ISO 10341 scope.
- [ ] Ethics approval before any human study.

---

### Send this with every STL to the print shop

| Use | Printer and settings |
|---|---|
| **Fixtures and frames** | FDM, PLA or PETG, 0.4 mm nozzle, 0.2 mm layers, 4 walls, 30 % infill. Print flat in the orientation the file is drawn, with no supports unless stated |
| **Gears, rings, small precise parts** | SLA resin, 0.05 mm layers |
| **Fit** | Holes and pockets are drawn with 0.3 mm clearance per side. If a test part is too tight or too loose, change `clr` in the file; do not scale the STL |
