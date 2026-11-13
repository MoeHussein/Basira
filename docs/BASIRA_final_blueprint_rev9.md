# BASIRA — final build blueprint (rev 9)

**Revision:** 7 November 2026. It supersedes rev 8 and is the plan to follow. Numbers marked † are reproduced by `verify.py`; "est." marks an estimate not yet measured.

## 0. Status

BASIRA is an off-patient research prototype. Nothing has been built or measured yet. Nothing goes on a person before gate G6 (§10). Every limit below is a proposed target; the optometrist confirms it at G0.

## 1. What BASIRA is

A lightweight, head-worn, app-controlled subjective refractor that an eye-care professional operates.

- **Outputs:** a spectacle prescription, and the corneal-plane power that feeds a contact-lens fitting. Corneal-plane power alone is not a contact-lens prescription; that still needs a fitting.
- **3D-printed parts:** housings, ring carriers, gears, bridge, covers and buckle.
- **Bought or moulded parts:** the optics, metal racks and pins, motors, sensors and electronics.
- **No liquid lenses.** Every optic is solid. That removes the two worst liquid-lens problems: gravity sag when the lens is upright, and temperature drift.

## 2. Final architecture (per eye)

**Optical order, eye to chart:** eye ← **Alvarez sphere plates** ← **−3.00 D cylinder** ← **+3.00 D cylinder** ← occluder ← front window ← chart.

| Function | Optic | Drive | Sensor |
|---|---|---|---|
| Sphere | Alvarez pair: two PMMA plates with complementary cubic surfaces. They slide **vertically**, in equal and opposite directions | 1 micro gearmotor driving a pinion between two metal racks (one per plate) | AS5048A on the pinion shaft. The magnet sits on-axis there, so the sensor's geometry fits |
| Cylinder and axis | ±3.00 D plano-cylinder trial lenses in two rotating rings (a Stokes pair) | 2 micro gearmotors | AS5048A on a spring-loaded 1:1 sense gear per ring |
| Occlusion | Flip flap | 1 micro servo | Commanded position, plus visual check |

**Per headset:** 6 gearmotors, 6 angle sensors and 2 servos.

**Why each choice:**
- **Alvarez for the sphere:** it is solid and thin. One motor sets it, and power is linear in shift, which keeps the app maths simple.
- **Vertical slide:** a horizontal slide would push the plates into the nose space.
- **Both plates move:** if only one plate moves, prism appears. A 6 mm one-plate shift gives 1.76 Δ (with A = 1000 m⁻²)†.
- **Stokes pair for cylinder and axis:** proven with trial-lens cylinders in the published prior art. It is cheap and its control law is closed-form.

**Fallback if the Alvarez plates fail G2:** a motorized wheel of trial-lens spheres, the way a conventional phoropter works. It is proven, but bulkier. The cylinder pair, app and electronics stay the same.

## 3. Control law

**Alvarez sphere.** Plate profile t = A(x·y² + x³/3), where x is the slide direction.
- Moving each plate by δ in opposite directions gives a pure sphere of **P = 4(n−1)·A·δ**†.
- The app commands **δ = (M − P₀)/k**, with k = 4(n−1)A and the offset P₀ both taken from calibration.

**Cylinder pair.** The target is (S, C ≤ 0, θ), minus-cylinder notation.
- Spherical equivalent set by the Alvarez sphere: **M = S + C/2**.
- Angle between the two cylinder rings: **Δ = arcsin(|C|/2P)**.
- Common ring angle: **(α+β)/2 = θ + 45°**.
- At C = 0 the axis is undefined; show "—".

This law was verified numerically to 5×10⁻¹⁵ D†.

**Refinement in software:**
- **JCC:** add a ±0.25 cross-cylinder (M = 0, |C| = 0.50 D) to the J vector. Only the rings move.
- **Fog:** add plus to M.
- **Chart distance:** distance Rx = measured − 1/L.

## 4. Tolerances

| Error source | Sensitivity† | Requirement |
|---|---|---|
| Alvarez slide position | k = 2.00 D/mm for a ±10 D design, 3.00 D/mm for ±15 D | 0.12 D needs a slide error ≤ 60 µm (±10 D) or ≤ 40 µm (±15 D). On a 5 mm pinion radius, 1° is 87 µm, so calibrate sensor non-linearity and pre-load the racks |
| Alvarez cross-slide offset | Cylinder = 4(n−1)A·e, independent of slide position: 0.5 mm → 1.00 D; 60 µm → 0.12 D | A **fixed** offset is a constant cylinder that calibration absorbs. **Play** must stay ≤ 60 µm, so use metal guide pins or a miniature rail, not printed slots |
| Alvarez plate twist | Adds cylinder | Measure at G2 |
| Ring differential angle (P = 3.00 D) | 0.105 D/° | ≤ 1.15° for 0.12 D: calibrate each AS5048A (raw ±0.8–1.2°) |
| Ring common angle (axis) | Residual cylinder = 2C·sin δ | 0.57° at 6 D, 1.7° at 2 D |
| BVD | 0.08–0.13 D/mm at ±10 D | Measure each exam to ±1 mm |
| PD decentration | 0.1·F Δ per mm | Lock and record |
| Temperature | PMMA dn/dT ≈ −1×10⁻⁴/K → ≈ 0.02 D per 10 K at 10 D (est.) | No compensation planned; G2 checks |

**Alvarez design table** (PMMA n = 1.49, 16 mm aperture)†:

| Range | Travel per plate | Plate sag p-v | k |
|---|---|---|---|
| ±6 D | ±5 mm | 1.9 mm | 1.20 D/mm |
| ±10 D | ±5 mm | 3.2 mm | 2.00 D/mm |
| ±15 D | ±5 mm | 4.8 mm | 3.00 D/mm |
| ±15 D | ±8 mm | 4.6 mm | 1.88 D/mm |

Longer travel relaxes the tolerances, at the cost of taller plates.

## 5. Reference plane and outputs

- **Reference plane:** the eye-side face of the first Alvarez plate.
- **Each exam:** measure the BVD (cornea to reference plane) with the side scale or a camera.
- **Conversion,** applied per principal meridian: F₂ = F₁ / (1 − (d₁ − d₂)·F₁), with d measured from the cornea in metres.
- **The app shows:**
  - device-plane power plus BVD;
  - the spectacle Rx at the stated BVD;
  - corneal-plane power, labelled "input to CL fitting".

## 6. Mechanics and packaging (estimates until CAD)

**Module size and gap (est.):**
- The module's outside diameter is ≈ 32–34 mm (26 mm cylinder lens plus ring), against 39.5 mm for the liquid lens in rev 8.
- That leaves a bare gap of ≈ 20–22 mm at 54 mm PD.

**Alvarez plates (est.):** about 22 × 30 mm, which covers a 16 mm aperture plus ±5 mm of travel.

**Stack depth, eye to window (est.):** ≈ 35–40 mm.

**Visor (est.):** ≈ 150 × 60 × 45 mm.

**Drives:**
- Cylinder rings rotate continuously with no stops.
- The Alvarez slide has hard stops and homes at power-up.
- Rack-and-pinion and guide pins are bought metal parts. Printed teeth are for the first prototype only.

**Face side:**
- Fixed eye-side barrier.
- Removable gasket.
- One-action strap release that works with the power off.

**First print:** a full-size dummy for fit, mass and centre of gravity. The PD gauges in `output/fixtures/` check gross geometry only.

## 7. Electronics, firmware and app

**Control chain:**
- Tablet app over a wired link (wireless later) to an ESP32-S3-DevKitC-1.
- 3 × DRV8833 drive the 6 gearmotors.
- The 6 AS5048A sensors share one SPI chain.
- The 2 servos run on MCU PWM.
- Power comes from a current-limited bench supply. Battery and patient isolation are not yet selected.

**States:** PARKED/INSPECTED → READY → MOVING → VERIFYING → CONFIRMED.

**CONFIRMED requires all of these:**
- all three drives of that eye are within tolerance;
- the calibration model is valid;
- BVD and PD are entered;
- the correct eye is selected.

**FAULT** (any violation):
- motion stops;
- the correction is marked *unknown*;
- mechanical removal still works.

**The app:**
- Controls: R/L selection; target SPH, CYL and AX with steps; JCC flips, fog and next chart line.
- Display: requested and confirmed values shown separately; the three outputs from §5.
- Record: the log plus "Mark incomplete" and "Save exam". The exam record stores every requested and confirmed value, BVD, PD, chart distance, model ID and faults.

## 8. Calibration model

**Fitted per eye:**
- the sphere: P(δ) = P₀ + k·δ (+ a small δ² term if the data need it);
- the cylinder lens powers P₊ and P₋;
- each ring's zero angle;
- constant J₀/J₄₅ offsets, which absorb any fixed Alvarez misalignment;
- the separations between elements (step-along power matrices, as in `verify.py`).

**Procedure:**
1. Take 30–60 lensmeter (or wavefront-bench) readings per eye.
2. Validate on ≥ 20 random held-out settings.
3. Store the model ID and the residuals.

The model becomes invalid after any module swap or failed check.

## 9. Bill of materials (no quotes yet)

| Item | Qty per headset | Route |
|---|---:|---|
| Alvarez plate pairs | 2 | **Prototype:** take them from adjustable-focus glasses that use two sliding plates. Confirm the type before buying; some adjustable glasses are fluid-filled. **Product:** custom diamond-turned or moulded PMMA, designed from §4 |
| ±3.00 D plano-cylinder trial lenses (+ and −) | 2 pairs | Buy; match on a lensmeter; edge to Ø ≈ 26 mm if needed |
| Micro metal gearmotors | 6 | Buy |
| AS5048A + Ø6 mm diametric magnets | 6 | Buy |
| Metal racks, pinions, guide pins, sense gears | 2 sets of racks; 6 gears | Buy (printed for the first prototype only) |
| Micro servos (occluders) | 2 | Buy |
| ESP32-S3-DevKitC-1, 3 × DRV8833 boards, connectors, harness | 1 set | Buy; make the harness |
| Housings, ring carriers, bridge with PD slide and lock, BVD side scale, covers, buckle | 1 set | 3D print |
| Strap and face gasket | 1 | Buy; qualified materials later |
| Lensmeter, trial lens set, chart display | — | Borrow |
| App, firmware, calibration fit | — | Make |

## 10. Gates — follow in order

**G0 — desk (this week):**
- The optometrist sets acceptance limits: power, axis, JCC flip time, BVD.
- Note prior art for the patent check: the Alvarez principle itself is from the 1960s; Humphrey's 1976 continuously variable refractor; the 2025 Valencia study.

**G1 — borrowed ±3.00 D trial cylinders on a lensmeter:**
1. Set Δ to 0, 10, 20, 30, 45, 60 and 90°, each at 2 common angles.
2. Compare with §3, and record the residual at Δ = 0.

**G2 — one pair of adjustable glasses, plates extracted:**
1. On the lensmeter, read power at 7 slide positions; repeat 3 times.
2. Fit k and P₀ and check the linearity residual.
3. Measure the range, the cylinder and prism at centre, and the cylinder change when the plates are pushed sideways.
- **Pass:** the plates become the sphere design reference.
- **Fail:** switch to the trial-lens sphere wheel.

**G3 — one-eye motorized bench:**
1. Run the 3 drives with their sensors.
2. Fit the calibration model and pass held-out validation.
3. Run the fault tests.

**G4 — two-eye dummy:**
- collisions;
- clear path;
- PD range;
- nose clearance;
- mass and centre of gravity.

**G5 — headform:**
- both eyes;
- occlusion;
- BVD measurement;
- JCC flip time;
- chart control;
- unpowered quick release.

A headform pass is not evidence of patient safety.

**G6 — product route:**
- custom Alvarez plates;
- freedom-to-operate search;
- intended use and regulatory scope (SFDA, ISO 10341);
- ethics approval before any human study;
- cost compared with the clinician's current kit.

**Still unknown:**
- the real Alvarez range, linearity and aberration;
- motor torque and lash;
- sensor accuracy after calibration;
- final packaging and mass;
- a schematic;
- firmware;
- clinical accuracy.

The accuracy benchmark to beat is the 2025 Valencia/Karolinska study (tunable sphere plus Stokes lens, on a chin rest). Against an optometrist it reached limits of agreement of M ±0.64 D, J0 ±0.36 D and J45 ±0.26 D.

---

## Appendix A — render prompt pack (concept visualization)

Renders are **visual intent only**. Label every image "concept render".

**Design spec (keep every render, and any CAD model, consistent with it):**

| Feature | Spec |
|---|---|
| Body | Slim horizontal visor, ≈ 150 × 60 × 45 mm, gently curved to the brow. Matte white medical polymer with a satin finish and fine parting lines |
| Palette | White and light grey #DDE4E2; charcoal #33444D for gasket and strap; teal #0F766E accent, used sparingly |
| Optics (front) | Two round windows, 20 mm clear aperture, centres 64 mm apart. Each window is ringed by a 34 mm module with two thin concentric rotating rings: the inner one brushed graphite with fine gear teeth, the outer one engraved with a 0–180° axis scale |
| Optics (behind the windows) | Clear flat plates, faintly visible |
| Motors | Small rounded motor housings on the outer (temple) side of each module |
| Bridge | Knurled teal PD wheel with a millimetre scale; deep nose cut-out |
| Top | Low centre pod with a tiny teal status LED and a tone-on-tone embossed "BASIRA" wordmark |
| Temples | Small clear side window with a millimetre ruler (vertex distance) |
| Back | Soft charcoal face gasket |
| Strap | Padded charcoal fabric strap with a teal quick-release buckle |
| Cables | None |

**1 · Hero product shot**
```
Photorealistic studio product render of a lightweight head-worn digital phoropter (an eye-refraction headset for optometrists) named BASIRA. Front three-quarter view at eye level, 85 mm lens, soft three-point lighting, seamless pale grey background, subtle floor reflection. A slim horizontal visor about 150 mm wide, 60 mm tall and 45 mm deep, gently curved, in matte white medical-grade polymer with satin finish and fine parting lines. Two round optical windows with 20 mm clear aperture, 64 mm apart, each set in a 34 mm circular module with two thin concentric rotating rings: inner ring brushed graphite with fine gear teeth, outer ring engraved with a 0 to 180 degree axis scale in fine black ticks; clear flat lens plates faintly visible inside. Small rounded motor housings on the outer side of each module. Knurled teal PD adjustment wheel with a millimetre scale on the nose bridge, deep nose cut-out. Low pod on top centre with a tiny teal status LED and the word BASIRA embossed tone-on-tone. Small clear side window with a millimetre ruler on each temple. Soft charcoal face gasket, padded charcoal fabric head strap with a teal quick-release buckle. Teal #0F766E accents used sparingly; white, light grey and charcoal elsewhere. Clean, precise, clinical, trustworthy, manufacturable by injection moulding, not futuristic.
```

**2 · Cutaway side view**
```
Same BASIRA headset, exact side profile, technical cutaway section through the right optical module along the line of sight, clean product-illustration render on white. From the eye side forward: soft charcoal face gasket, two thin clear PMMA Alvarez lens plates in vertical metal guide slides with a small brass pinion between two racks, a minus cylinder lens in a geared rotating ring, a plus cylinder lens in a geared rotating ring, a small flip occluder, a clear front window. Tiny gearmotors beneath each ring, small sensor chips beside each gear. Light grey section hatching, crisp edges, white and light grey materials, teal accents.
```

**3 · Exploded view**
```
Exploded isometric view of the BASIRA headset's right optical module, parts floating apart along the optical axis with thin dashed alignment lines, white background, studio lighting: face gasket, eye-side barrier, two Alvarez plates with racks and pinion, minus and plus cylinder lenses in geared rings, three micro gearmotors with sense gears and sensor chips, servo flip occluder, front window and white module cover. Precise, minimal, product-design presentation style.
```

**4 · In use**
```
Bright modern optometry clinic. An adult patient sits upright wearing the white BASIRA head-worn refraction headset described above, strap around the head. An optometrist beside them holds a tablet showing a clean exam app with large SPH, CYL and AXIS values and Right/Left buttons. A visual acuity chart glows on a wall display about six metres away, softly out of focus. Natural window light, documentary photography, calm and professional.
```

**Negative prompt (all views):** VR headset, screens on the front, sci-fi glow, neon, cables, extra lenses, other brand logos, extra or garbled text, distorted lettering, binoculars, goggles strap clutter, toy-like proportions.

**Workflow:**
1. Generate the hero first.
2. Feed the best hero image back in as the image reference for views 2–4, so all four views show the same object.
3. For exact geometry, model the design spec in CAD or Blender and use these prompts as the lighting and material brief.

## Sources

- Stokes lens and the DIY refraction benchmark: [PLoS One 2025](https://pmc.ncbi.nlm.nih.gov/articles/PMC12530553/)
- Stokes-lens validation: [Opt. Eng. 2022](https://doi.org/10.1117/1.OE.61.12.121803)
- Humphrey's refractor: [Opt. Eng. 1976](https://doi.org/10.1117/12.7971977)
- AS5048A on-axis sensor: [ams](https://ams-osram.com/products/sensor-solutions/position-sensors/ams-as5048a-)
- Phoropter reference range: [Topcon VT-10](https://www.veatchinstruments.com/topcon-vt-10-vision-tester.aspx)
- Fluid-lens auto-phoropter (2017): [Sci. Rep.](https://doi.org/10.1038/s41598-017-14507-5)
- Folding Phoropter (sphere only): [GitHub](https://github.com/buildfreak93/Folding-Phoropter-App)
