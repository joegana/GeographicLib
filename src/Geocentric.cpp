/**
 * \file Geocentric.cpp
 * \brief Implementation for GeographicLib::Geocentric class
 *
 * Copyright (c) Charles Karney (2008, 2009, 2010, 2011) <charles@karney.com>
 * and licensed under the MIT/X11 License.  For more information, see
 * http://geographiclib.sourceforge.net/
 **********************************************************************/

#include <GeographicLib/Geocentric.hpp>

#define GEOGRAPHICLIB_GEOCENTRIC_CPP "$Id: b0ff568567c01468c2238235842d09ab8be527fe $"

RCSID_DECL(GEOGRAPHICLIB_GEOCENTRIC_CPP)
RCSID_DECL(GEOGRAPHICLIB_GEOCENTRIC_HPP)

namespace GeographicLib {

  using namespace std;

  Geocentric::Geocentric(real a, real f)
    : _a(a)
    , _f(f <= 1 ? f : 1/f)
    , _e2(_f * (2 - _f))
    , _e2m(Math::sq(1 - _f))          // 1 - _e2
    , _e2a(abs(_e2))
    , _e4a(Math::sq(_e2))
    , _maxrad(2 * _a / numeric_limits<real>::epsilon())
  {
    if (!(Math::isfinite(_a) && _a > 0))
      throw GeographicErr("Major radius is not positive");
    if (!(Math::isfinite(_f) && _f < 1))
      throw GeographicErr("Minor radius is not positive");
  }

  const Geocentric Geocentric::WGS84(Constants::WGS84_a<real>(),
                                     Constants::WGS84_f<real>());

  void Geocentric::IntForward(real lat, real lon, real h,
                              real& x, real& y, real& z,
                              real M[dim2_]) const throw() {
    lon = lon >= 180 ? lon - 360 : (lon < -180 ? lon + 360 : lon);
    real
      phi = lat * Math::degree<real>(),
      lam = lon * Math::degree<real>(),
      sphi = sin(phi),
      cphi = abs(lat) == 90 ? 0 : cos(phi),
      n = _a/sqrt(1 - _e2 * Math::sq(sphi)),
      slam = lon == -180 ? 0 : sin(lam),
      clam = abs(lon) == 90 ? 0 : cos(lam);
    z = ( _e2m * n + h) * sphi;
    x = (n + h) * cphi;
    y = x * slam;
    x *= clam;
    if (M)
      Rotation(sphi, cphi, slam, clam, M);
  }

  void Geocentric::IntReverse(real x, real y, real z,
                              real& lat, real& lon, real& h,
                              real M[dim2_]) const throw() {
    real
      R = Math::hypot(x, y),
      slam = R ? y / R : 0,
      clam = R ? x / R : 1;
    h = Math::hypot(R, z);      // Distance to center of earth
    real sphi, cphi;
    if (h > _maxrad) {
      // We really far away (> 12 million light years); treat the earth as a
      // point and h, above, is an acceptable approximation to the height.
      // This avoids overflow, e.g., in the computation of disc below.  It's
      // possible that h has overflowed to inf; but that's OK.
      //
      // Treat the case x, y finite, but R overflows to +inf by scaling by 2.
      R = Math::hypot(x/2, y/2);
      slam = R ? (y/2) / R : 0;
      clam = R ? (x/2) / R : 1;
      real H = Math::hypot(z/2, R);
      sphi = (z/2) / H;
      cphi = R / H;
    } else if (_e4a == 0) {
      // Treat the spherical case.  Dealing with underflow in the general case
      // with _e2 = 0 is difficult.  Origin maps to N pole same as with
      // ellipsoid.
      real H = Math::hypot(h == 0 ? 1 : z, R);
      sphi = (h == 0 ? 1 : z) / H;
      cphi = R / H;
      h -= _a;
    } else {
      // Treat prolate spheroids by swapping R and z here and by switching
      // the arguments to phi = atan2(...) at the end.
      real
        p = Math::sq(R / _a),
        q = _e2m * Math::sq(z / _a),
        r = (p + q - _e4a) / 6;
      if (_f < 0) swap(p, q);
      if ( !(_e4a * q == 0 && r <= 0) ) {
        real
          // Avoid possible division by zero when r = 0 by multiplying
          // equations for s and t by r^3 and r, resp.
          S = _e4a * p * q / 4, // S = r^3 * s
          r2 = Math::sq(r),
          r3 = r * r2,
          disc =  S * (2 * r3 + S);
        real u = r;
        if (disc >= 0) {
          real T3 = S + r3;
          // Pick the sign on the sqrt to maximize abs(T3).  This minimizes
          // loss of precision due to cancellation.  The result is unchanged
          // because of the way the T is used in definition of u.
          T3 += T3 < 0 ? -sqrt(disc) : sqrt(disc); // T3 = (r * t)^3
          // N.B. cbrt always returns the real root.  cbrt(-8) = -2.
          real T = Math::cbrt(T3); // T = r * t
          // T can be zero; but then r2 / T -> 0.
          u += T + (T != 0 ? r2 / T : 0);
        } else {
          // T is complex, but the way u is defined the result is real.
          real ang = atan2(sqrt(-disc), -(S + r3));
          // There are three possible cube roots.  We choose the root which
          // avoids cancellation.  Note that disc < 0 implies that r < 0.
          u += 2 * r * cos(ang / 3);
        }
        real
          v = sqrt(Math::sq(u) + _e4a * q), // guaranteed positive
          // Avoid loss of accuracy when u < 0.  Underflow doesn't occur in
          // e4 * q / (v - u) because u ~ e^4 when q is small and u < 0.
          uv = u < 0 ? _e4a * q / (v - u) : u + v, // u+v, guaranteed positive
          // Need to guard against w going negative due to roundoff in uv - q.
          w = max(real(0), _e2a * (uv - q) / (2 * v)),
          // Rearrange expression for k to avoid loss of accuracy due to
          // subtraction.  Division by 0 not possible because uv > 0, w >= 0.
          k = uv / (sqrt(uv + Math::sq(w)) + w),
          k1 = _f >= 0 ? k : k - _e2,
          k2 = _f >= 0 ? k + _e2 : k,
          d = k1 * R / k2,
          H = Math::hypot(z/k1, R/k2);
        sphi = (z/k1) / H;
        cphi = (R/k2) / H;
        h = (1 - _e2m/k1) * Math::hypot(d, z);
      } else {                  // e4 * q == 0 && r <= 0
        // This leads to k = 0 (oblate, equatorial plane) and k + e^2 = 0
        // (prolate, rotation axis) and the generation of 0/0 in the general
        // formulas for phi and h.  using the general formula and division by 0
        // in formula for h.  So handle this case by taking the limits:
        // f > 0: z -> 0, k      ->   e2 * sqrt(q)/sqrt(e4 - p)
        // f < 0: R -> 0, k + e2 -> - e2 * sqrt(q)/sqrt(e4 - p)
        real
          zz = sqrt((_f >= 0 ? _e4a - p : p) / _e2m),
          xx = sqrt( _f <  0 ? _e4a - p : p        ),
          H = Math::hypot(zz, xx);
        sphi = zz / H;
        cphi = xx / H;
        if (z < 0) sphi = -sphi; // for tiny negative z (not for prolate)
        h = - _a * (_f >= 0 ? _e2m : 1) * H / _e2a;
      }
    }
    lat = atan2(sphi, cphi) / Math::degree<real>();
    // Negative signs return lon in [-180, 180).
    lon = -atan2(-slam, clam) / Math::degree<real>();
    if (M)
      Rotation(sphi, cphi, slam, clam, M);
  }

  void Geocentric::Rotation(real sphi, real cphi, real slam, real clam,
                            real M[dim2_]) throw() {
    // This rotation matrix is given by the following quaternion operations
    // qrot(lam, [0,0,1]) * qrot(phi, [0,-1,0]) * [1,1,1,1]/2
    // or
    // qrot(pi/2 + lam, [0,0,1]) * qrot(-pi/2 + phi , [-1,0,0])
    // where
    // qrot(t,v) = [cos(t/2), sin(t/2)*v[1], sin(t/2)*v[2], sin(t/2)*v[3]]

    // Local x axis (east) in geocentric coords
    M[0] = -slam;        M[3] =  clam;        M[6] = 0;
    // Local y axis (north) in geocentric coords
    M[1] = -clam * sphi; M[4] = -slam * sphi; M[7] = cphi;
    // Local z axis (up) in geocentric coords
    M[2] =  clam * cphi; M[5] =  slam * cphi; M[8] = sphi;
  }

} // namespace GeographicLib
