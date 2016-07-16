/***************************************************************************
 *
 * Multitouch X driver
 * Copyright (C) 2008 Henrik Rydberg <rydberg@euromail.se>
 * Copyright (C) 2011 Ryan Bourgeois <bluedragonx@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 **************************************************************************/

#include "mtouch.h"
#include "mprops.h"
#include "capabilities.h"
#include "os.h"
#include "trig.h" /* xorg/os.h for timers */

#include <xf86Module.h>
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
#include <X11/Xatom.h>
#include <xserver-properties.h>
#endif

#define TAP_HOLD 100
#define TAP_TIMEOUT 200
#define TAP_THRESHOLD 0.05
#define TICK_TIMEOUT 50
#define SCROLL_THRESHOLD 0.05
#define SWIPE_THRESHOLD 0.15
#define SCALE_THRESHOLD 0.15
#define ROTATE_THRESHOLD 0.15

#define NUM_AXES 4


#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 12
typedef InputInfoPtr LocalDevicePtr;
#endif

/* button mapping simplified */
#define PROPMAP(m, x, y) m[x] = XIGetKnownProperty(y)

static void pointer_control(DeviceIntPtr dev, PtrCtrl *ctrl)
{
#if DEBUG_DRIVER
	xf86Msg(X_INFO, "pointer_control\n");
#endif
}

#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
static void initAxesLabels(Atom map[NUM_AXES])
{
	memset(map, 0, NUM_AXES * sizeof(Atom));
	PROPMAP(map, 0, AXIS_LABEL_PROP_REL_X);
	PROPMAP(map, 1, AXIS_LABEL_PROP_REL_Y);
	PROPMAP(map, 2, AXIS_LABEL_PROP_REL_HSCROLL);
	PROPMAP(map, 3, AXIS_LABEL_PROP_REL_VSCROLL);
}

static void initButtonLabels(Atom map[DIM_BUTTON])
{
	memset(map, 0, DIM_BUTTON * sizeof(Atom));
	PROPMAP(map, MT_BUTTON_LEFT, BTN_LABEL_PROP_BTN_LEFT);
	PROPMAP(map, MT_BUTTON_MIDDLE, BTN_LABEL_PROP_BTN_MIDDLE);
	PROPMAP(map, MT_BUTTON_RIGHT, BTN_LABEL_PROP_BTN_RIGHT);
	PROPMAP(map, MT_BUTTON_WHEEL_UP, BTN_LABEL_PROP_BTN_WHEEL_UP);
	PROPMAP(map, MT_BUTTON_WHEEL_DOWN, BTN_LABEL_PROP_BTN_WHEEL_DOWN);
	PROPMAP(map, MT_BUTTON_HWHEEL_LEFT, BTN_LABEL_PROP_BTN_HWHEEL_LEFT);
	PROPMAP(map, MT_BUTTON_HWHEEL_RIGHT, BTN_LABEL_PROP_BTN_HWHEEL_RIGHT);
	/* how to map swipe buttons? */
	PROPMAP(map, MT_BUTTON_SWIPE_UP, BTN_LABEL_PROP_BTN_0);
	PROPMAP(map, MT_BUTTON_SWIPE_DOWN, BTN_LABEL_PROP_BTN_1);
	PROPMAP(map, MT_BUTTON_SWIPE_LEFT, BTN_LABEL_PROP_BTN_2);
	PROPMAP(map, MT_BUTTON_SWIPE_RIGHT, BTN_LABEL_PROP_BTN_3);
	/* how to map scale and rotate? */
	PROPMAP(map, MT_BUTTON_SCALE_DOWN, BTN_LABEL_PROP_BTN_4);
	PROPMAP(map, MT_BUTTON_SCALE_UP, BTN_LABEL_PROP_BTN_5);
	PROPMAP(map, MT_BUTTON_ROTATE_LEFT, BTN_LABEL_PROP_BTN_6);
	PROPMAP(map, MT_BUTTON_ROTATE_RIGHT, BTN_LABEL_PROP_BTN_7);
}
#endif

static int device_init(DeviceIntPtr dev, LocalDevicePtr local)
{
	struct MTouch *mt = local->private;
	unsigned char btmap[DIM_BUTTON + 1] = {
		0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
	};
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
	Atom axes_labels[NUM_AXES], btn_labels[DIM_BUTTON];
	initAxesLabels(axes_labels);
	initButtonLabels(btn_labels);
#endif

	local->fd = xf86OpenSerial(local->options);
	if (local->fd < 0) {
		xf86Msg(X_ERROR, "mtrack: cannot open device\n");
		return !Success;
	}
	if (mtouch_configure(mt, local->fd)) {
		xf86Msg(X_ERROR, "mtrack: cannot configure device\n");
		return !Success;
	}
	xf86CloseSerial(local->fd);

#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) < 3
	InitPointerDeviceStruct((DevicePtr)dev,
				btmap, DIM_BUTTON,
				GetMotionHistory,
				pointer_control,
				GetMotionHistorySize(),
				2);
#elif GET_ABI_MAJOR(ABI_XINPUT_VERSION) < 7
	InitPointerDeviceStruct((DevicePtr)dev,
				btmap, DIM_BUTTON,
				pointer_control,
				GetMotionHistorySize(),
				2);
#elif GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
	InitPointerDeviceStruct((DevicePtr)dev,
				btmap, DIM_BUTTON, btn_labels,
				pointer_control,
				GetMotionHistorySize(),
				NUM_AXES, axes_labels);
#else
#error "Unsupported ABI_XINPUT_VERSION"
#endif

	xf86InitValuatorAxisStruct(dev, 0,
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
				   axes_labels[0],
#endif
				   mt->caps.abs[MTDEV_POSITION_X].minimum,
				   mt->caps.abs[MTDEV_POSITION_X].maximum,
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 12
				   1, 0, 1, Absolute);
#else
				   1, 0, 1);
#endif
	xf86InitValuatorDefaults(dev, 0);
	xf86InitValuatorAxisStruct(dev, 1,
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
				   axes_labels[1],
#endif
				   mt->caps.abs[MTDEV_POSITION_Y].minimum,
				   mt->caps.abs[MTDEV_POSITION_Y].maximum,
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 12
				   1, 0, 1, Absolute);
#else
				   1, 0, 1);
#endif
	xf86InitValuatorDefaults(dev, 1);
	mprops_init(&mt->cfg, local);
	SetScrollValuator(dev, 2, SCROLL_TYPE_HORIZONTAL, mt->cfg.scroll.dist, 0);
	SetScrollValuator(dev, 3, SCROLL_TYPE_VERTICAL, mt->cfg.scroll.dist, 0);
	XIRegisterPropertyHandler(dev, mprops_set_property, NULL, NULL);

	TimerInit();
	mt->timer = NULL; /* allocated later */
	mt->is_timer_installed = 0;
	mt->absolute_mode = FALSE;
	return Success;
}

static int device_on(LocalDevicePtr local)
{
	struct MTouch *mt = local->private;
	local->fd = xf86OpenSerial(local->options);
	if (local->fd < 0) {
		xf86Msg(X_ERROR, "mtrack: cannot open device\n");
		return !Success;
	}
	if (mtouch_open(mt, local->fd)) {
		xf86Msg(X_ERROR, "mtrack: cannot grab device\n");
		return !Success;
	}
	xf86AddEnabledDevice(local);
	if(mt->timer != NULL)
		TimerFree(mt->timer);	// release any existing timer
	mt->timer = NULL;
	mt->is_timer_installed = 0;
	return Success;
}

static int device_off(LocalDevicePtr local)
{
	struct MTouch *mt = local->private;
	xf86RemoveEnabledDevice(local);
	if (mtouch_close(mt))
		xf86Msg(X_WARNING, "mtrack: cannot ungrab device\n");
	xf86CloseSerial(local->fd);
	if(mt->timer != NULL)
		TimerFree(mt->timer);	// release any existing timer
	mt->timer = NULL;
	mt->is_timer_installed = 0;
	return Success;
}

static int device_close(LocalDevicePtr local)
{
	return Success;
}

static void set_and_post_mask(struct MTouch *mt, DeviceIntPtr dev,
                              mstime_t delta_t){
	struct Gestures* gs;
	ValuatorMask* mask;
	float speed_factor;

	gs = &mt->gs;
	mask = mt->vm;

	valuator_mask_zero(mask);

	if (gs->move_dx)
		valuator_mask_set_double(mask, 0, gs->move_dx);
	if (gs->move_dy)
		valuator_mask_set_double(mask, 1, gs->move_dy);

	switch (gs->move_type){
	case GS_SCROLL:
	case GS_SWIPE3:
	case GS_SWIPE4:
	//case GS_MOVE:
	case GS_NONE:
		/* Only scroll, or swipe or move can trigger coasting right now */
		/* Continue coasting if enabled */
//#speed_factor = (mt->cfg.scroll_coast.num_of_ticks - gs->scroll_coast_tick_no) / (float)(mt->cfg.scroll_coast.num_of_ticks);
		speed_factor = gs->scroll_coast_tick_no;
		if (ABSVAL(gs->scroll_speed_x) > mt->cfg.scroll_coast.min_speed)
			valuator_mask_set_double(mask, 2, gs->scroll_speed_x * delta_t * speed_factor);
		if (ABSVAL(gs->scroll_speed_y) > mt->cfg.scroll_coast.min_speed)
			valuator_mask_set_double(mask, 3, gs->scroll_speed_y * delta_t * speed_factor);

		break;

	default:
		/* Any other movement/gesture type will break coasting. */
		mt->gs.scroll_speed_y = mt->gs.scroll_speed_x = 0.0f;
		break;
	}

	xf86PostMotionEventM(dev, Relative, mask);

	/* Once posted, we can clear the move variables */
	gs->move_dx = gs->move_dy = 0;
}

static void handle_gestures(LocalDevicePtr local,
			struct Gestures* gs)
{
	struct MTouch *mt = local->private;
	static bitmask_t buttons_prev = 0U;
	int i;

	if(mt->absolute_mode == FALSE){
		if (mt->cfg.scroll_smooth){
			/* Copy states from button_prev into current buttons state */
			MODBIT(gs->buttons, 3, GETBIT(buttons_prev, 3));
			MODBIT(gs->buttons, 4, GETBIT(buttons_prev, 4));
			MODBIT(gs->buttons, 5, GETBIT(buttons_prev, 5));
			MODBIT(gs->buttons, 6, GETBIT(buttons_prev, 6));
			set_and_post_mask(mt, local->dev, timertoms(&gs->dt));
		}
		else{
			// mt->absolute_mode == FALSE
			if (gs->move_dx != 0 || gs->move_dy != 0)
				xf86PostMotionEvent(local->dev, 0, 0, 2, gs->move_dx, gs->move_dy);
		}
	}
	else{
		/* Give the HW coordinates to Xserver as absolute coordinates, these coordinates
		 * are not scaled, this is oke if the touchscreen has the same resolution as the display.
		 */
		xf86PostMotionEvent(local->dev, 1, 0, 2,
			mt->state.touch[0].x + get_cap_xmid(&mt->caps),
			mt->state.touch[0].y + get_cap_ymid(&mt->caps));
	}

	for (i = 0; i < 32; i++) {
		if (GETBIT(gs->buttons, i) == GETBIT(buttons_prev, i))
			continue;
		if (GETBIT(gs->buttons, i)) {
			xf86PostButtonEvent(local->dev, FALSE, i+1, 1, 0, 0);
#define DEBUG_DRIVER 0
#if DEBUG_DRIVER
			xf86Msg(X_INFO, "button %d down\n", i+1);
#endif
		}
		else {
			xf86PostButtonEvent(local->dev, FALSE, i+1, 0, 0, 0);
#if DEBUG_DRIVER
			xf86Msg(X_INFO, "button %d up\n", i+1);
#endif
#undef DEBUG_DRIVER
		}
	}
	buttons_prev = gs->buttons;
}

static CARD32 coasting_delayed(OsTimerPtr timer, CARD32 time, void *arg){
  LocalDevicePtr local = arg;
	struct MTouch *mt = local->private;

	xf86Msg(X_ERROR, "coasting_delayed: tick_no: %d speed_x=%f, speed_y=%f, dir=%i\n",
            mt->gs.scroll_coast_tick_no,
            mt->gs.scroll_speed_x, mt->gs.scroll_speed_y, mt->gs.move_dir);
#if DEBUG_DRIVER
	xf86Msg(X_INFO, "coasting_delayed: speed_x=%f, speed_y=%f, dir=%i\n", mt->gs.scroll_speed_x, mt->gs.scroll_speed_y, mt->gs.move_dir);
#endif
	set_and_post_mask(mt, local->dev, mt->cfg.scroll_coast.tick_ms);

	if (mt->gs.scroll_coast_tick_no <= 0){
		mt->gs.scroll_speed_x = mt->gs.scroll_speed_y = 0.0;
		mt->is_timer_installed = 0;
		TimerCancel(mt->timer);
		return 0;
	}

	--mt->gs.scroll_coast_tick_no;
	TimerSet(mt->timer, 0, mt->cfg.scroll_coast.tick_ms, coasting_delayed, local);
	mt->is_timer_installed = 2;

	return 0;
}

/*
 * Timers documentation:
 * http://www.x.org/releases/X11R7.7/doc/xorg-server/Xserver-spec.html#id2536042
 *
 * This function indirectly may call itself recursively using timer to guarantee correct
 * event delivery time. Ususally recursion ends after first recursive call.
 */
static CARD32 check_resolve_delayed(OsTimerPtr timer, CARD32 time, void *arg){
	LocalDevicePtr local = arg;
	struct MTouch *mt = local->private;
	mstime_t delta_millis;
	struct timeval delta;

	// If it was to early to trigger delayed button, next timer will be set,
	// but when called by timer such situation shouldn't take place.
	switch (mtouch_delayed(mt)){
	case 1:
		if(mt->is_timer_installed != 1){
			TimerCancel(mt->timer);
			mt->is_timer_installed = 1;
			timersub(&mt->gs.button_delayed_time, &mt->gs.time, &delta);
			delta_millis = timertoms(&delta);
			mt->timer = TimerSet(mt->timer, 0, delta_millis, check_resolve_delayed, local);
		}
		break;
	case 2:
		TimerCancel(mt->timer);
		mt->is_timer_installed = 0;
		handle_gestures(local, &mt->gs);
		break;
	case 3:
		TimerCancel(mt->timer);
		handle_gestures(local, &mt->gs);
		mt->is_timer_installed = 2;

		/* Install coasting timer */
		coasting_delayed(mt->timer, -1, arg);
		break;
	case 0: break;
	}
	return 0;
}

/*
 *  Called for each full received packet from the touchpad.
 * Any xf86 input event generated by int this callback function fill be queued by
 * xorg server, and fired when control return from this function.
 * So to fire event as early as possible this function should return quickly.
 * For delayed events we can't simply wait in this function, because it will delay
 * all events generated by 'handle_gestures'.
 * Moreover we don't know when next input event will occur, so to guarantee proper
 * timing I have to use timer.
 *
 * If mtouch_delayed() retured 1 this means it was to early to trigger delayed button,
 * and new timer have to be installed. Otherwise events generated inside can be handled
 * as usual.
 *
 * More on input event processing:
 * http://www.x.org/wiki/Development/Documentation/InputEventProcessing/
 */
static void read_input(LocalDevicePtr local)
{
	struct MTouch *mt = local->private;
	while (mtouch_read(mt) > 0)
		handle_gestures(local, &mt->gs);
	check_resolve_delayed(NULL, 0, local);
}

static int switch_mode(ClientPtr client, DeviceIntPtr dev, int mode)
{
	LocalDevicePtr local = dev->public.devicePrivate;
	struct MTouch *mt = local->private;

	switch (mode) {
	case Absolute:
		mt->absolute_mode = TRUE;
		xf86Msg(X_INFO, "switch_mode: switing to absolute mode\n");
		break;
	case Relative:
		mt->absolute_mode = FALSE;
		xf86Msg(X_INFO, "switch_mode: switing to relative mode\n");
		break;
	default:
		return XI_BadMode;
	}
	return Success;
}


static Bool device_control(DeviceIntPtr dev, int mode)
{
	LocalDevicePtr local = dev->public.devicePrivate;
	switch (mode) {
	case DEVICE_INIT:
		xf86Msg(X_INFO, "device control: init\n");
		return device_init(dev, local);
	case DEVICE_ON:
		xf86Msg(X_INFO, "device control: on\n");
		return device_on(local);
	case DEVICE_OFF:
		xf86Msg(X_INFO, "device control: off\n");
		return device_off(local);
	case DEVICE_CLOSE:
		xf86Msg(X_INFO, "device control: close\n");
		return device_close(local);
	default:
		xf86Msg(X_INFO, "device control: default\n");
		return BadValue;
	}
}


#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 12
static int preinit(InputDriverPtr drv, InputInfoPtr pInfo, int flags)
{
	struct MTouch *mt;

	mt = calloc(1, sizeof(*mt));
	if (!mt)
		return BadAlloc;

	pInfo->private = mt;
	pInfo->type_name = XI_TOUCHPAD;
	pInfo->device_control = device_control;
	pInfo->read_input = read_input;
	pInfo->switch_mode = switch_mode;

	xf86CollectInputOptions(pInfo, NULL);
	xf86OptionListReport(pInfo->options);
	xf86ProcessCommonOptions(pInfo, pInfo->options);
	mconfig_configure(&mt->cfg, pInfo->options);
	mt->vm = valuator_mask_new(1);

	return Success;
}
#else
static InputInfoPtr preinit(InputDriverPtr drv, IDevPtr dev, int flags)
{
	struct MTouch *mt;
	InputInfoPtr local = xf86AllocateInput(drv, 0);
	if (!local)
		goto error;
	mt = calloc(1, sizeof(struct MTouch));
	if (!mt)
		goto error;

	local->name = dev->identifier;
	local->type_name = XI_TOUCHPAD;
	local->device_control = device_control;
	local->read_input = read_input;
	local->switch_mode = switch_mode;
	local->private = mt;
	local->flags = XI86_POINTER_CAPABLE | XI86_SEND_DRAG_EVENTS;
	local->conf_idev = dev;

	xf86CollectInputOptions(local, NULL, NULL);
	xf86OptionListReport(local->options);
	xf86ProcessCommonOptions(local, local->options);
	mconfig_configure(&mt->cfg, local->options);
	mt->vm = NULL;

	local->flags |= XI86_CONFIGURED;
 error:
	return local;
}
#endif

static void uninit(InputDriverPtr drv, InputInfoPtr local, int flags)
{
	struct MTouch *mt = local->private;

	if (mt->vm)
		valuator_mask_free(&mt->vm);
	free(local->private);
	local->private = 0;
	xf86DeleteInput(local, 0);
}

/* About xorg drivers, modules:
 * http://www.x.org/wiki/Development/Documentation/XorgInputHOWTO/
 */
static InputDriverRec MTRACK = {
	1,
	"mtrack",
	NULL,
	preinit,
	uninit,
	NULL,
	0
};

static XF86ModuleVersionInfo moduleVersion = {
	"mtrack",
	MODULEVENDORSTRING,
	MODINFOSTRING1,
	MODINFOSTRING2,
	XORG_VERSION_CURRENT,
	PACKAGE_VERSION_MAJOR, PACKAGE_VERSION_MINOR, PACKAGE_VERSION_PATCHLEVEL,
	ABI_CLASS_XINPUT,
	ABI_XINPUT_VERSION,
	MOD_CLASS_XINPUT,
	{0, 0, 0, 0}
};

static pointer setup(pointer module, pointer options, int *errmaj, int *errmin)
{
	xf86AddInputDriver(&MTRACK, module, 0);
	return module;
}

_X_EXPORT XF86ModuleData mtrackModuleData = {&moduleVersion, &setup, NULL };
