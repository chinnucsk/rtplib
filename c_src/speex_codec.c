#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include "erl_driver.h"
#include <speex/speex.h>

typedef struct {
	ErlDrvPort port;
	SpeexBits bits;
	void* estate;
	void* dstate;
} codec_data;

enum {
	CMD_ENCODE = 1,
	CMD_DECODE = 2
};

/* http://tools.ietf.org/html/rfc5574 */
/* FIXME hardcoded */
#define FRAME_SIZE 160

static ErlDrvData codec_drv_start(ErlDrvPort port, char *buff)
{
	codec_data* d = (codec_data*)driver_alloc(sizeof(codec_data));
	d->port = port;
	speex_bits_init(&d->bits);
	/* FIXME hardcoded narrowband mode (speex_wb_mode, speex_uwb_mode) */
	d->estate = speex_encoder_init(&speex_nb_mode);
	d->dstate = speex_decoder_init(&speex_nb_mode);
	set_port_control_flags(port, PORT_CONTROL_FLAG_BINARY);
	return (ErlDrvData)d;
}

static void codec_drv_stop(ErlDrvData handle)
{
	codec_data *d = (codec_data *) handle;
	speex_bits_destroy(&d->bits);
	speex_encoder_destroy(d->estate);
	speex_decoder_destroy(d->dstate);
	driver_free((char*)handle);
}

static int codec_drv_control(
		ErlDrvData handle,
		unsigned int command,
		char *buf, int len,
		char **rbuf, int rlen)
{
	codec_data* d = (codec_data*)handle;

	int i;
	int ret = 0;
	ErlDrvBinary *out;
	*rbuf = NULL;
	float frame[FRAME_SIZE];
	char cbits[200];

	switch(command) {
		case CMD_ENCODE:
			for (i=0; i < len / 2; i++){
				frame[i] = (buf[2*i] & 0xff) | (buf[2*i+1] << 8);
			}
			speex_bits_reset(&d->bits);
			speex_encode(d->estate, frame, &d->bits);
			ret = speex_bits_write(&d->bits, cbits, 200);
			out = driver_alloc_binary(ret);
			memcpy(out->orig_bytes, cbits, ret);
			*rbuf = (char *) out;
			break;
		 case CMD_DECODE:
			out = driver_alloc_binary(2*FRAME_SIZE);
			speex_bits_read_from(&d->bits, buf, len);
			speex_decode_int(d->dstate, &d->bits, (short *)(out->orig_bytes));
			*rbuf = (char *) out;
			ret = 2*FRAME_SIZE;
			break;
		 default:
			break;
	}
	return ret;
}

ErlDrvEntry codec_driver_entry = {
	NULL,			/* F_PTR init, N/A */
	codec_drv_start,	/* L_PTR start, called when port is opened */
	codec_drv_stop,		/* F_PTR stop, called when port is closed */
	NULL,			/* F_PTR output, called when erlang has sent */
	NULL,			/* F_PTR ready_input, called when input descriptor ready */
	NULL,			/* F_PTR ready_output, called when output descriptor ready */
	"speex_codec_drv",		/* char *driver_name, the argument to open_port */
	NULL,			/* F_PTR finish, called when unloaded */
	NULL,			/* handle */
	codec_drv_control,	/* F_PTR control, port_command callback */
	NULL,			/* F_PTR timeout, reserved */
	NULL			/* F_PTR outputv, reserved */
};

DRIVER_INIT(codec_drv) /* must match name in driver_entry */
{
	return &codec_driver_entry;
}