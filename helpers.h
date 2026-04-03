// Float-validaattori — käytetään kaikkialla
inline bool validf(float x) { return !isnan(x) && isfinite(x); }