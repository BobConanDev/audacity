/**********************************************************************

  Audacity: A Digital Audio Editor

  TimeFormat.cpp

  Dominic Mazzoni
  Michael Papadopoulos split from Ruler.h

**********************************************************************/

#include "TimeFormat.h"
#include "RealFormat.h"

const TimeFormat &TimeFormat::Instance()
{
   static TimeFormat instance;
   return instance;
}

void TimeFormat::SetTickSizes(
   double units, double& mMajor, double& mMinor, double& mMinorMinor,
   int& mDigits
) const
{
   if (units > 0.5) {
      if (units < 1.0) { // 1 sec
         mMinor = 1.0;
         mMajor = 5.0;
         return;
      }
      if (units < 5.0) { // 5 sec
         mMinor = 5.0;
         mMajor = 15.0;
         return;
      }
      if (units < 10.0) {
         mMinor = 10.0;
         mMajor = 30.0;
         return;
      }
      if (units < 15.0) {
         mMinor = 15.0;
         mMajor = 60.0;
         return;
      }
      if (units < 30.0) {
         mMinor = 30.0;
         mMajor = 60.0;
         return;
      }
      if (units < 60.0) { // 1 min
         mMinor = 60.0;
         mMajor = 300.0;
         return;
      }
      if (units < 300.0) { // 5 min
         mMinor = 300.0;
         mMajor = 900.0;
         return;
      }
      if (units < 600.0) { // 10 min
         mMinor = 600.0;
         mMajor = 1800.0;
         return;
      }
      if (units < 900.0) { // 15 min
         mMinor = 900.0;
         mMajor = 3600.0;
         return;
      }
      if (units < 1800.0) { // 30 min
         mMinor = 1800.0;
         mMajor = 3600.0;
         return;
      }
      if (units < 3600.0) { // 1 hr
         mMinor = 3600.0;
         mMajor = 6 * 3600.0;
         return;
      }
      if (units < 6 * 3600.0) { // 6 hrs
         mMinor = 6 * 3600.0;
         mMajor = 24 * 3600.0;
         return;
      }
      if (units < 24 * 3600.0) { // 1 day
         mMinor = 24 * 3600.0;
         mMajor = 7 * 24 * 3600.0;
         return;
      }

      mMinor = 24.0 * 7.0 * 3600.0; // 1 week
      mMajor = 24.0 * 7.0 * 3600.0;
      return;
   }

   // Otherwise fall through to RealFormat
   // (fractions of a second should be dealt with
   // the same way as for RealFormat)
   RealFormat::LinearInstance().SetTickSizes(
      units, mMajor, mMinor, mMinorMinor, mDigits);
}

void TimeFormat::SetLabelString(
   wxString& s, double d, double mMinor, int mDigits, TickType tickType
) const
{
   // Replace -0 with 0
   if (d < 0.0 && (d + mMinor > 0.0))
      d = 0.0;

   if (tickType == RulerFormat::t_major) {
      if (d < 0) {
         s = wxT("-");
         d = -d;
      }

#if ALWAYS_HH_MM_SS
      int secs = (int)(d + 0.5);
      if (mMinor >= 1.0) {
         s.Printf(wxT("%d:%02d:%02d"), secs / 3600, (secs / 60) % 60, secs % 60);
      }
      else {
         wxString t1, t2, format;
         t1.Printf(wxT("%d:%02d:"), secs / 3600, (secs / 60) % 60);
         format.Printf(wxT("%%0%d.%dlf"), mDigits + 3, mDigits);
         t2.Printf(format, fmod(d, 60.0));
         s += t1 + t2;
      }
      break;
#endif

      if (mMinor >= 3600.0) {
         int hrs = (int)(d / 3600.0 + 0.5);
         wxString h;
         h.Printf(wxT("%d:00:00"), hrs);
         s += h;
      }
      else if (mMinor >= 60.0) {
         int minutes = (int)(d / 60.0 + 0.5);
         wxString m;
         if (minutes >= 60)
            m.Printf(wxT("%d:%02d:00"), minutes / 60, minutes % 60);
         else
            m.Printf(wxT("%d:00"), minutes);
         s += m;
      }
      else if (mMinor >= 1.0) {
         int secs = (int)(d + 0.5);
         wxString t;
         if (secs >= 3600)
            t.Printf(wxT("%d:%02d:%02d"), secs / 3600, (secs / 60) % 60, secs % 60);
         else if (secs >= 60)
            t.Printf(wxT("%d:%02d"), secs / 60, secs % 60);
         else
            t.Printf(wxT("%d"), secs);
         s += t;
      }
      else {
         // Commented out old and incorrect code for avoiding the 40mins and 60 seconds problem
         // It was causing Bug 463 - Incorrect Timeline numbering (where at high zoom and long tracks,
         // numbers did not change.
#if 0
            // The casting to float is working around an issue where 59 seconds
            // would show up as 60 when using g++ (Ubuntu 4.3.3-5ubuntu4) 4.3.3.
         int secs = (int)(float)(d);
         wxString t1, t2, format;

         if (secs >= 3600)
            t1.Printf(wxT("%d:%02d:"), secs / 3600, (secs / 60) % 60);
         else if (secs >= 60)
            t1.Printf(wxT("%d:"), secs / 60);

         if (secs >= 60)
            format.Printf(wxT("%%0%d.%dlf"), mDigits + 3, mDigits);
         else
            format.Printf(wxT("%%%d.%dlf"), mDigits + 3, mDigits);
         // The casting to float is working around an issue where 59 seconds
         // would show up as 60 when using g++ (Ubuntu 4.3.3-5ubuntu4) 4.3.3.
         t2.Printf(format, fmod((float)d, (float)60.0));
#else
            // For d in the range of hours, d is just very slightly below the value it should 
            // have, because of using a double, which in turn yields values like 59:59:999999 
            // mins:secs:nanosecs when we want 1:00:00:000000
            // so adjust by less than a nano second per hour to get nicer number formatting.
         double dd = d * 1.000000000000001;
         int secs = (int)(dd);
         wxString t1, t2, format;

         if (secs >= 3600)
            t1.Printf(wxT("%d:%02d:"), secs / 3600, (secs / 60) % 60);
         else if (secs >= 60)
            t1.Printf(wxT("%d:"), secs / 60);

         if (secs >= 60)
            format.Printf(wxT("%%0%d.%dlf"), mDigits + 3, mDigits);
         else
            format.Printf(wxT("%%%d.%dlf"), mDigits + 3, mDigits);
         // dd will be reduced to just the seconds and fractional part.
         dd = dd - secs + (secs % 60);
         // truncate to appropriate number of digits, so that the print formatting 
         // doesn't round up 59.9999999 to 60.
         double multiplier = pow(10, mDigits);
         dd = ((int)(dd * multiplier)) / multiplier;
         t2.Printf(format, dd);
#endif
         s += t1 + t2;
      }
   }
}

TimeFormat::~TimeFormat() = default;