import numpy as np
np.set_printoptions(precision=4, suppress=True)
deg=np.pi/180

# ---------- 1. UV fused silica dispersion (Malitson 1965 Sellmeier) ----------
def n_fs(lam_um):
    l2=lam_um**2
    return np.sqrt(1+0.6961663*l2/(l2-0.0684043**2)+0.4079426*l2/(l2-0.1162414**2)+0.8974794*l2/(l2-9.896161**2))
P355=1/0.300
for name,lam in [("355",0.355),("e 546.07",0.54607),("d 587.56",0.58756)]:
    n=n_fs(lam); print(f"UVFS n({name})={n:.5f}  power of 300mm@355 lens = {P355*(n-1)/(n_fs(0.355)-1):.3f} D")
Pvis=P355*(n_fs(0.54607)-1)/(n_fs(0.355)-1)
print(f"Max Stokes cyl with EKSMA pair (e-line): {2*Pvis:.2f} D\n")

# ---------- 2. Stokes pair: power matrices and closed-form control law ----------
def cylF(P,ax):  # thin cylinder, power P, axis ax (power meridian = ax+90)
    m=ax+np.pi/2; c,s=np.cos(m),np.sin(m)
    return P*np.array([[c*c,c*s],[c*s,s*s]])
def sphF(S): return S*np.eye(2)
def to_rx(F):  # -> (S, C<=0, axis deg) minus-cyl
    w,v=np.linalg.eigh(F)        # w ascending
    S=w[1]; C=w[0]-w[1]
    ax=np.degrees(np.arctan2(v[1,1],v[0,1]))%180   # axis = meridian of S (max power)
    return S,C,ax
def pv(S,C,ax):  # Thibos power vectors
    a=ax*deg; return np.array([S+C/2,-C/2*np.cos(2*a),-C/2*np.sin(2*a)])
def control(S,C,ax,P):  # target minus-cyl Rx -> (sphere-lens power, alpha(+P), beta(-P))
    D=np.arcsin(min(1,abs(C)/(2*P)))
    mean=ax*deg+np.pi/4
    return S+C/2, mean+D/2, mean-D/2
rng=np.random.default_rng(1); P=3.0; worst=0
for _ in range(20000):
    S=rng.uniform(-15,15); C=-rng.uniform(0,2*P); ax=rng.uniform(0,180)
    Ms,a,b=control(S,C,ax,P)
    F=sphF(Ms)+cylF(P,a)+cylF(-P,b)
    worst=max(worst,np.abs(pv(*to_rx(F))-pv(S,C,ax)).max())
print(f"Closed-form Stokes control law, max power-vector error over 20k random targets: {worst:.2e} D")
print("  sphere lens M = S + C/2 ; Delta = alpha-beta = asin(|C|/2P) ; (alpha+beta)/2 = axis + 45 deg\n")

# ---------- 3. Angle-error sensitivity ----------
for P in (1.5,3.0,Pvis):
    print(f"P=+/-{P:.2f} D: Cmax={2*P:.2f} D, dC/dDelta at C=0 = {2*P*deg:.3f} D/deg; "
          f"differential error for 0.12 D near zero = {0.12/(2*P*deg):.2f} deg")
for C in (0.5,2,6):
    print(f"  axis error giving 0.12 D residual cyl at C={C} D: {np.degrees(np.arcsin(0.12/(2*C))):.2f} deg")
print()

# ---------- 4. Vertex sensitivity ----------
print("Corneal-plane power Fc = F/(1-dF), F = back-vertex power, d = BVD")
for F in (-10,-5,5,10):
    r=[F/(1-d*F) for d in (0.010,0.012,0.014,0.020)]
    print(f"  F={F:+} D  d=10/12/14/20 mm -> " + "  ".join(f"{x:+.2f}" for x in r)+f"   dFc/dd at 12mm = {F**2/(1-0.012*F)**2/1000:.3f} D/mm")
print()

# ---------- 5. Stack with separations (dioptric power matrices, object at infinity) ----------
def propagate(elements):  # list of (F 2x2, distance to next element in m); returns back-vertex vergence matrix
    L=np.zeros((2,2))
    for F,d in elements:
        L=L+F
        if d>0: L=L@np.linalg.inv(np.eye(2)-d*L)
    return L
# order A: eye <- sphere(nearest) ... cyl pair farther.  Light goes far->near, so list far element first.
gap=0.008; t_sph=0.0186
for S,C,ax in [(-8,-2,30),(+6,-4,100),(0,-6,0),(-15,-1,170)]:
    Ms,a,b=control(S,C,ax,3.0)
    near_sph=propagate([(cylF(3,a),0.005),(cylF(-3,b),gap),(sphF(Ms),0)])
    far_sph =propagate([(sphF(Ms),gap+t_sph/2),(cylF(3,a),0.005),(cylF(-3,b),0)])
    e1=pv(*to_rx(near_sph))-pv(S,C,ax); e2=pv(*to_rx(far_sph))-pv(S,C,ax)
    print(f"target {S:+.2f}/{C:+.2f}x{ax:3.0f}: thin-sum error  sphere-nearest-eye M,J0,J45={e1}  sphere-farthest={e2}")
print()

# ---------- 6. Gravity coma -> power gradient across aperture ----------
# W = c*sqrt(8)*(3 rho^3 - 2 rho) sin(theta), rho=r/R.  Curvature terms: W = a*y*(x^2+y^2)+tilt, a=3*sqrt(8)*c/R^3
# local W_yy=6 a y0, W_xx=2 a y0  -> M=-(Wxx+Wyy)/2=-4 a y0 , J0=-(Wxx-Wyy)/2=2 a y0
def coma_grad(c_rms_m,R_m):
    a=3*np.sqrt(8)*c_rms_m/R_m**3
    return 4*a/1000, 2*a/1000   # D/mm
lam=532e-9
for label,wfe_h,wfe_v,R in [("EL-16-40-20D (H 2.5, V 0.5 lambda)",2.5,0.5,0.8*8e-3),("EL-16-40-5D (H 0.5, V 0.25)",0.5,0.25,0.8*8e-3)]:
    coma=np.sqrt(wfe_h**2-wfe_v**2)*lam
    gM,gJ=coma_grad(coma,R)
    print(f"{label}: if excess is pure coma ({coma*1e6:.2f} um RMS over R={R*1e3:.1f} mm) -> dM/dy={gM:.3f} D/mm, dJ0/dy={gJ:.3f} D/mm")
# numerical check of the derivative
R=6.4e-3; c=1e-6; a=3*np.sqrt(8)*c/R**3
W=lambda x,y: c*np.sqrt(8)*(3*((x*x+y*y)**1.5)/R**3*(y/np.sqrt(x*x+y*y+1e-30)) - 2*y/R)
h=1e-5; y0=1e-3
Wyy=(W(0,y0+h)-2*W(0,y0)+W(0,y0-h))/h**2; Wxx=(W(h,y0)-2*W(0,y0)+W(-h,y0))/h**2
print(f"  numeric check: Wyy={Wyy:.3f} vs 6ay0={6*a*y0:.3f};  Wxx={Wxx:.3f} vs 2ay0={2*a*y0:.3f}  (1/m)\n")

# ---------- 7. Packaging + Prentice ----------
for OD in (39.5,40.0):
    print(f"OD {OD} mm: bare gap at PD 50/54/58 = " + ", ".join(f"{pd-OD:.1f}" for pd in (50,54,58)) + " mm")
print(f"25.4 mm square sweep diameter = {25.4*np.sqrt(2):.2f} mm")
for F in (5,10,15):
    print(f"Prentice: 1 mm decentration at {F} D -> {0.1*F:.1f} prism dioptres")

# ---------- 8. Alvarez sphere pair (final architecture) ----------
# plate profile t(x,y) = A*(x*y^2 + x^3/3) [m], second plate = -t. Slide axis x (can be built vertical).
print("\nALVAREZ SPHERE PAIR")
xs=np.linspace(-0.008,0.008,41); X,Y=np.meshgrid(xs,xs)
t=lambda x,y,A: A*(x*y**2+x**3/3)
def fit_quad(T):   # fit T = c0 + cx*x + cy*y + c2*(x^2+y^2) + residual
    M=np.column_stack([np.ones(X.size),X.ravel(),Y.ravel(),(X**2+Y**2).ravel()])
    c,res,*_=np.linalg.lstsq(M,T.ravel(),rcond=None); return c, np.abs(T.ravel()-M@c).max()
n=1.49; A=1000.0; d=0.003
c,r=fit_quad(t(X-d,Y,A)-t(X+d,Y,A))           # symmetric: each plate moves d, opposite ways
print(f"symmetric shift +/-{d*1e3:.0f} mm: P = {-2*c[3]*(n-1):+.3f} D (formula 4(n-1)A*d = {4*(n-1)*A*d:.3f}), wedge x-term {c[1]:.2e}, residual {r:.1e} m")
c,r=fit_quad(t(X-2*d,Y,A)-t(X,Y,A))           # one plate moves 2d, other fixed
print(f"one-plate shift {2*d*1e3:.0f} mm: P = {-2*c[3]*(n-1):+.3f} D, wedge x-term {c[1]:.2e} -> prism {abs(c[1])*(n-1)*100:.2f} prism dioptres")
c,r=fit_quad(t(X-d,Y-0.0005,A)-t(X+d,Y,A))    # 0.5 mm cross-slide misalignment of one plate
print(f"0.5 mm cross misalignment adds wedge y-term {c[2]:.2e} -> {abs(c[2])*(n-1)*100:.2f} prism dioptres plus cylinder (residual {r:.1e} m)")
print("Design table, PMMA n=1.49, peak-to-peak sag of one plate over aperture+travel:")
for R,dmax,a in [(10,0.005,0.008),(15,0.005,0.008),(15,0.008,0.008),(6,0.005,0.008)]:
    A=R/(4*(n-1)*dmax); L=a+dmax
    pv=2*A*(L*a**2+L**3/3)
    print(f"  +/-{R} D, travel +/-{dmax*1e3:.0f} mm/plate, aperture {2*a*1e3:.0f} mm: A={A:.0f} /m^2, sag p-v {pv*1e3:.1f} mm, dP/dshift {4*(n-1)*A/1000:.2f} D/mm, 0.12 D -> {0.12/(4*(n-1)*A/1000)*1000:.0f} um")
def fit_full(T):   # T = c0 + cx x + cy y + a x^2 + b y^2 + c xy
    M=np.column_stack([np.ones(X.size),X.ravel(),Y.ravel(),(X**2).ravel(),(Y**2).ravel(),(X*Y).ravel()])
    k,*_=np.linalg.lstsq(M,T.ravel(),rcond=None); _,cx,cy,a,b,c=k
    H=np.array([[2*a,c],[c,2*b]])*(n-1); w=np.linalg.eigvalsh(H)
    return abs(w[1]-w[0]), np.hypot(cx,cy)*(n-1)*100
A=1020.0
for d in (0.0,0.003):
    for e in (0.0005,0.00006):
        cyl,prism=fit_full(t(X-d,Y-e,A)-t(X+d,Y,A))
        print(f"A={A:.0f}: cross-slide offset {e*1e6:.0f} um at shift {d*1e3:.0f} mm -> cylinder {cyl:.3f} D, prism {prism:.2f} pd"
              f"  (rule: 4(n-1)A*e = {4*(n-1)*A*e:.3f} D)")
