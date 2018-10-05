#include <assert.h>

#include <LCUI_Build.h>
#include <LCUI/LCUI.h>
#include <LCUI/timer.h>

#include "animation.h"

static void Animation_OnFrame(Animation ani)
{
	unsigned duration;
	
	if (ani->_state != ANIMATION_STATE_STARTED) {
		return;
	}
	duration = (unsigned)LCUI_GetTimeDelta(ani->_start_time);
	if (duration >= 0 && duration < ani->duration) {
		ani->_progress = duration * 1.0 / ani->duration;
	} else {
		ani->_progress = 1.0;
	}
	ani->on_frame(ani);
	if (duration >= ani->duration) {
		Animation_Stop(ani);
	}
}

static void Animation_OnPlay(Animation ani)
{
	unsigned ms = max(1000 / LCUI_MAX_FRAMES_PER_SEC, 5);

	assert(ani->duration > ms);
	ani->_state = ANIMATION_STATE_STARTED;
	ani->_start_time = LCUI_GetTime();
	ani->_timer = LCUI_SetInterval(ms, Animation_OnFrame, ani);
}

void Animation_Play(Animation ani)
{
	assert(ani->on_frame != NULL);

	ani->_progress = 0;
	ani->_state = ANIMATION_STATE_DELAY;
	if (ani->delay == 0) {
		Animation_OnPlay(ani);
		return;
	}
	if (ani->_timer) {
		LCUITimer_Reset(ani->_timer, ani->delay);
		return;
	}
	ani->_timer = LCUI_SetTimeout(ani->delay, Animation_OnPlay, ani);
}

void Animation_Stop(Animation ani)
{
	if (ani->_state != ANIMATION_STATE_STARTED) {
		return;
	}
	if (ani->_timer) {
		LCUITimer_Free(ani->_timer);
	}
	ani->_timer = 0;
	ani->_state = ANIMATION_STATE_FINISHED;
	if (ani->on_finished) {
		ani->on_finished(ani);
	}
}
