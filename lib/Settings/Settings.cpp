/**
 * @file   Settings.cpp
 */
#include "Settings.hpp"
#include <Preferences.h>

bool Settings::begin()
{
  Preferences prefs;
  if (!prefs.begin(NAMESPACE, true))
  {
    restoreDefaults();
    return false;
  }
  Blob         blob = {};
  const size_t length = prefs.getBytes(KEY, &blob, sizeof(blob));
  GainsBlob    gainsBlob = {};
  const size_t gainsLength = prefs.isKey(GAINS_KEY) ? prefs.getBytes(GAINS_KEY, &gainsBlob, sizeof(gainsBlob)) : 0;
  const float  hoverTilt = prefs.isKey(HOVER_KEY) ? prefs.getFloat(HOVER_KEY, 0.0f) : 0.0f;
  prefs.end();
  m_hoverTiltDeg = isfinite(hoverTilt) ? hoverTilt : 0.0f;

  m_gains =
      ((gainsLength == sizeof(gainsBlob)) && (gainsBlob.version == GAINS_VERSION)) ? gainsBlob.gains : StoredGains{};

  if ((length != sizeof(blob)) || (blob.version != VERSION))
  {
    m_data = {0.0f, 0.0f, 0, 0};  // keep any gains that were loaded
    return false;
  }
  m_data = blob.data;
  return true;
}

bool Settings::save()
{
  Preferences prefs;
  if (!prefs.begin(NAMESPACE, false))
  {
    return false;
  }
  const Blob blob = {VERSION, m_data};
  bool       ok = prefs.putBytes(KEY, &blob, sizeof(blob)) == sizeof(blob);
  if (m_gains.valid)
  {
    const GainsBlob gainsBlob = {GAINS_VERSION, m_gains};
    ok &= prefs.putBytes(GAINS_KEY, &gainsBlob, sizeof(gainsBlob)) == sizeof(gainsBlob);
  }
  else if (prefs.isKey(GAINS_KEY))
  {
    prefs.remove(GAINS_KEY);
  }
  ok &= prefs.putFloat(HOVER_KEY, m_hoverTiltDeg) == sizeof(float);
  prefs.end();
  return ok;
}

void Settings::restoreDefaults()
{
  m_data = {0.0f, 0.0f, 0, 0};
  m_gains = {};
  m_hoverTiltDeg = 0.0f;
}
