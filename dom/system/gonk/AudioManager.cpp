/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is B2G Audio Manager.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Philipp von Weitershausen <philipp@weitershausen.de>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <android/log.h> 

#include "mozilla/Hal.h"
#include "AudioManager.h"
#include <pulse/pulseaudio.h>

using namespace mozilla::dom::gonk;
using namespace mozilla::hal;
using namespace mozilla;

#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "AudioManager" , ## args) 

NS_IMPL_ISUPPORTS1(AudioManager, nsIAudioManager)

static AudioManager *gManager = NULL;

static void
InternalSetAudioRoutes(SwitchState aState)
{
  if (aState == SWITCH_STATE_ON) {
    gManager->SetAudioRoute(nsIAudioManager::FORCE_HEADPHONES);
  } else if (aState == SWITCH_STATE_OFF) {
    gManager->SetAudioRoute(nsIAudioManager::FORCE_SPEAKER);
  }
}

class HeadphoneSwitchObserver : public SwitchObserver
{
public:
  void Notify(const SwitchEvent& aEvent) {
    InternalSetAudioRoutes(aEvent.status());
  }
};

AudioManager::AudioManager()
  : mPhoneState(PHONE_STATE_CURRENT)
  , mObserver(new HeadphoneSwitchObserver())
  , mCBMonitor("AudioManager")
{
  gManager = this;

  mThreadedMainloop = pa_threaded_mainloop_new();
  MOZ_ASSERT(mThreadedMainloop);

  pa_threaded_mainloop_start(mThreadedMainloop);

  mContext = pa_context_new(pa_threaded_mainloop_get_api(mThreadedMainloop),
                            "Gecko AudioManager");
  MOZ_ASSERT(mContext);

  pa_context_connect(mContext, NULL, PA_CONTEXT_NOAUTOSPAWN, NULL);

  RegisterSwitchObserver(SWITCH_HEADPHONES, mObserver);
  
  InternalSetAudioRoutes(GetCurrentSwitchState(SWITCH_HEADPHONES));
}

AudioManager::~AudioManager() {
  UnregisterSwitchObserver(SWITCH_HEADPHONES, mObserver);

  pa_context_disconnect(mContext);
  pa_context_unref(mContext);
  mContext = NULL;

  pa_threaded_mainloop_stop(mThreadedMainloop);
  pa_threaded_mainloop_free(mThreadedMainloop);
  mThreadedMainloop = NULL;
}

NS_IMETHODIMP
AudioManager::GetMicrophoneMuted(bool* aMicrophoneMuted)
{
//  if (AudioSystem::isMicrophoneMuted(aMicrophoneMuted)) {
//    return NS_ERROR_FAILURE;
//  }
  return NS_OK;
}

NS_IMETHODIMP
AudioManager::SetMicrophoneMuted(bool aMicrophoneMuted)
{
//  if (AudioSystem::muteMicrophone(aMicrophoneMuted)) {
//    return NS_ERROR_FAILURE;
//  }
  return NS_OK;
}

NS_IMETHODIMP
AudioManager::GetMasterVolume(float* aMasterVolume)
{
  *aMasterVolume = mMasterVolume;
  return NS_OK;
}

void
AudioManager::SuccessCB(pa_context *c, int success, void *userdata)
{
  AudioManager *manager = static_cast<AudioManager *>(userdata);
  MonitorAutoLock lock(manager->mCBMonitor);
  manager->mSuccess = success;
  lock.Notify();
}

NS_IMETHODIMP
AudioManager::SetMasterVolume(float aMasterVolume)
{
  pa_cvolume vol;
  pa_cvolume_init(&vol);
  pa_cvolume_set(&vol, 2, pa_volume_t(PA_VOLUME_NORM * aMasterVolume));

  pa_operation *op = pa_context_set_sink_volume_by_index(mContext, 0, &vol, SuccessCB, this);

  if (!op)
    return NS_ERROR_FAILURE;

  MonitorAutoLock lock(mCBMonitor);
  lock.Wait();
  pa_operation_unref(op);
  mMasterVolume = aMasterVolume;

  return mSuccess ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
AudioManager::GetMasterMuted(bool* aMasterMuted)
{
//  if (AudioSystem::getMasterMute(aMasterMuted)) {
//    return NS_ERROR_FAILURE;
//  }
  return NS_OK;
}

NS_IMETHODIMP
AudioManager::SetMasterMuted(bool aMasterMuted)
{
//  if (AudioSystem::setMasterMute(aMasterMuted)) {
//    return NS_ERROR_FAILURE;
//  }
  return NS_OK;
}

NS_IMETHODIMP
AudioManager::GetPhoneState(PRInt32* aState)
{
  *aState = mPhoneState;
  return NS_OK;
}

NS_IMETHODIMP
AudioManager::SetPhoneState(PRInt32 aState)
{
//  if (AudioSystem::setPhoneState(aState)) {
//    return NS_ERROR_FAILURE;
//  }
  return NS_OK;
}

//
// Kids, don't try this at home.  We want this to link and work on
// both GB and ICS.  Problem is, the symbol exported by audioflinger
// is different on the two gonks.
//
// So what we do here is weakly link to both of them, and then call
// whichever symbol resolves at dynamic link time (if any).
//
NS_IMETHODIMP
AudioManager::SetForceForUse(PRInt32 aUsage, PRInt32 aForce)
{
#if 0
  status_t status = 0;
  if (static_cast<
      status_t (*)(AudioSystem::force_use, AudioSystem::forced_config)
      >(AudioSystem::setForceUse)) {
    // Dynamically resolved the GB signature.
    status = AudioSystem::setForceUse((AudioSystem::force_use)aUsage,
                                      (AudioSystem::forced_config)aForce);
  } else if (static_cast<
             status_t (*)(audio_policy_force_use_t, audio_policy_forced_cfg_t)
             >(AudioSystem::setForceUse)) {
    // Dynamically resolved the ICS signature.
    status = AudioSystem::setForceUse((audio_policy_force_use_t)aUsage,
                                      (audio_policy_forced_cfg_t)aForce);
  }
  return status ? NS_ERROR_FAILURE : NS_OK;
#endif
  return NS_OK;
}

NS_IMETHODIMP
AudioManager::GetForceForUse(PRInt32 aUsage, PRInt32* aForce) {
#if 0
  if (static_cast<
      AudioSystem::forced_config (*)(AudioSystem::force_use)
      >(AudioSystem::getForceUse)) {
    // Dynamically resolved the GB signature.
    *aForce = AudioSystem::getForceUse((AudioSystem::force_use)aUsage);
  } else if (static_cast<
             audio_policy_forced_cfg_t (*)(audio_policy_force_use_t)
             >(AudioSystem::getForceUse)) {
    // Dynamically resolved the ICS signature.
    *aForce = AudioSystem::getForceUse((audio_policy_force_use_t)aUsage);
  }
#endif
  return NS_OK;
}

void
AudioManager::SetAudioRoute(int aRoutes) {
  const char *device;

  switch (aRoutes) {
  case nsIAudioManager::FORCE_HEADPHONES:
    device = "Headset";
    break;
  case nsIAudioManager::FORCE_SPEAKER:
  default:
    device = "Handsfree";
    break;
  }

  pa_operation *op = pa_context_set_sink_port_by_index(mContext, 0, device, SuccessCB, this);

  if (!op)
    return;

  MonitorAutoLock lock(mCBMonitor);
  lock.Wait();
  pa_operation_unref(op);

  return;
}
