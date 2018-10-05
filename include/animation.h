#ifndef LCUI_ANIMATION_H
#define LCUI_ANIMATION_H

typedef enum AnimationState_ {
	ANIMATION_STATE_FINISHED,
	ANIMATION_STATE_STARTED,
	ANIMATION_STATE_DELAY
} AnimationState;

typedef struct AnimationRec_  AnimationRec;
typedef struct AnimationRec_*  Animation;

struct AnimationRec_ {
	/* public members, requires user to manually set */

	unsigned duration;
	unsigned delay;
	void(*on_frame)(Animation);
	void(*on_finished)(Animation);
	void *data;

	/* private members, user readonly */

	int _timer;
	double _progress;
	int64_t _start_time;
	AnimationState _state;
};

void Animation_Play(Animation ani);
void Animation_Stop(Animation ani);

#endif
