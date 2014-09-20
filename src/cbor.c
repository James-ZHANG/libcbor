#include "cbor.h"
#include "cbor_internal.h"
#include <assert.h>
#include <zlib.h>

/* TODO refactor the metadata madness using structs and unions */

void cbor_incref(cbor_item_t * item)
{
	item->refcount++;
}

void cbor_decref(cbor_item_t ** item)
{
	if (--(*item)->refcount == 0) {
		switch((*item)->type) {
		case CBOR_TYPE_UINT:
			/* Fixed size, simple free suffices */
			/* Fallthrough */
		case CBOR_TYPE_NEGINT:
			/* Fixed size, simple free suffices */
			{
				free((*item)->data);
				break;
			}
		case CBOR_TYPE_BYTESTRING:
			{
				if (cbor_bytestring_is_definite(*item)) {
					free((*item)->data);
				}
				break;
			}
		case CBOT_TYPE_STRING:
		case CBOR_TYPE_ARRAY:
			{
				/* Get all items and decref them */
				cbor_item_t ** handle = cbor_array_handle(*item);
				for (size_t i = 0; i < cbor_array_get_size(*item); i++)
					cbor_decref(&handle[i]);
				/* Fallthrough */
			}
		case CBOR_TYPE_MAP:
		case CBOR_TYPE_TAG:
			{
				free((*item)->data);
				break;
			}
		case CBOR_TYPE_FLOAT_CTRL:
			{
				// unless it's a control symbol
				if (cbor_float_ctrl_get_width(*item) != CBOR_FLOAT_0)
					free((*item)->data);
				break;
			}
		}
		free(*item);
		//TODO
		*item = NULL;
	}
}


cbor_item_t * cbor_load(const unsigned char * source,
						size_t source_size,
						cbor_flags_t flags,
						struct cbor_load_result * result)
{
	if (source_size < 1) {
		result->error = (struct cbor_error) { 0, CBOR_ERR_NODATA };
		return NULL;
	}
	// TODO fuse malloc fro simple types

	cbor_item_t * res = malloc(sizeof(cbor_item_t));
	res->refcount = 1;
	result->read = 1; /* We always attempt to read the MTB */
	result->error = (struct cbor_error) { 0, CBOR_ERR_NONE };

	switch (*source) {
	case 0x00:
	case 0x01:
	case 0x02:
	case 0x03:
	case 0x04:
	case 0x05:
	case 0x06:
	case 0x07:
	case 0x08:
	case 0x09:
	case 0x0A:
	case 0x0B:
	case 0x0C:
	case 0x0D:
	case 0x0E:
	case 0x0F:
	case 0x10:
	case 0x11:
	case 0x12:
	case 0x13:
	case 0x14:
	case 0x15:
	case 0x16:
	case 0x17:
	/* Embedded one byte uint */
		{
			res->type = CBOR_TYPE_UINT;
			_cbor_handle_load_uint8(source, source_size, res, result);
			result->read--; /* Restart the handler at the MTB */
			break;
		}
	/* One byte uint */
	case 0x18:
		{
			res->type = CBOR_TYPE_UINT;
			_cbor_handle_load_uint8(source + 1, source_size - 1, res, result);
			break;
		}
	/* Two byte uint */
	case 0x19:
		{
			res->type = CBOR_TYPE_UINT;
			_cbor_handle_load_uint16(source + 1, source_size - 1, res, result);
			break;
		}
	/* Four byte uint */
	case 0x1A:
		{
			res->type = CBOR_TYPE_UINT;
			_cbor_handle_load_uint32(source + 1, source_size - 1, res, result);
			break;
		}
	/* Glorious eight byte uint */
	case 0x1B:
		{
			res->type = CBOR_TYPE_UINT;
			_cbor_handle_load_uint64(source + 1, source_size - 1, res, result);
			break;
		}
	case 0x20:
	case 0x21:
	case 0x22:
	case 0x23:
	case 0x24:
	case 0x25:
	case 0x26:
	case 0x27:
	case 0x28:
	case 0x29:
	case 0x2A:
	case 0x2B:
	case 0x2C:
	case 0x2D:
	case 0x2E:
	case 0x2F:
	case 0x30:
	case 0x31:
	case 0x32:
	case 0x33:
	case 0x34:
	case 0x35:
	case 0x36:
	case 0x37:
		/* Embedded one byte negint */
		{
			res->type = CBOR_TYPE_NEGINT;
			_cbor_handle_load_uint8(source, source_size, res, result);
			result->read--; /* Restart the handler at the MTB */
			cbor_set_uint8(res, cbor_get_uint8(res) - 0x20); /* Offset by 0x20 */
			break;
		}
	case 0x38:
		/* One byte negint */
		{
			res->type = CBOR_TYPE_NEGINT;
			_cbor_handle_load_uint8(source + 1, source_size - 1, res, result);
			break;
		}
	case 0x39:
		/* Two byte negint */
		{
			res->type = CBOR_TYPE_NEGINT;
			_cbor_handle_load_uint16(source + 1, source_size - 1, res, result);
			break;
		}
	case 0x3A:
		/* Four byte negint */
		{
			res->type = CBOR_TYPE_NEGINT;
			_cbor_handle_load_uint32(source + 1, source_size - 1, res, result);
			break;
		}
	case 0x3B:
		/* Eight byte negint */
		{
			res->type = CBOR_TYPE_NEGINT;
			_cbor_handle_load_uint64(source + 1, source_size - 1, res, result);
			break;
		}
	/* RESERVED - 0x3C - 0x3F*/
	case 0x40:
	case 0x41:
	case 0x42:
	case 0x43:
	case 0x44:
	case 0x45:
	case 0x46:
	case 0x47:
	case 0x48:
	case 0x49:
	case 0x4A:
	case 0x4B:
	case 0x4C:
	case 0x4D:
	case 0x4E:
	case 0x4F:
	case 0x50:
	case 0x51:
	case 0x52:
	case 0x53:
	case 0x54:
	case 0x55:
	case 0x56:
	case 0x57:
		/* Byte string with embedded length */
		{
			res->type = CBOR_TYPE_BYTESTRING;
			uint8_t length = _cbor_load_uint8(source) - 0x40; /* Offset by 0x40 */
			_cbor_handle_load_bytestring(source + 1, source_size - 1, length, res, result);
			break;
		}
	case 0x58:
		/* Bytestring with one byte length */
		{
			res->type = CBOR_TYPE_BYTESTRING;
			/* We have read one byte, need at least one more for the uint8_t */
			if (!_cbor_assert_avail_bytes(1, source_size - 1, result)) {
				result->error.position = 1;
			} else {
				result->read++;
				uint8_t length = _cbor_load_uint8(source + 1);
				_cbor_handle_load_bytestring(source + 2, source_size - 2, length, res, result);
			}
			break;
		}
	case 0x59:
		/* Bytestring with two bytes length */
		{
			res->type = CBOR_TYPE_BYTESTRING;
			/* We have read one byte, need at least one more for the uint8_t */
			if (!_cbor_assert_avail_bytes(2, source_size - 1, result)) {
				result->error.position = 1;
			} else {
				result->read += 2;
				uint16_t length = _cbor_load_uint16(source + 1);
				_cbor_handle_load_bytestring(source + 3, source_size - 3, length, res, result);
			}
			break;
		}
	case 0x5A:
		/* Bytestring with four bytes length */
		{
			res->type = CBOR_TYPE_BYTESTRING;
			/* We have read one byte, need at least one more for the uint8_t */
			if (!_cbor_assert_avail_bytes(4, source_size - 1, result)) {
				result->error.position = 1;
			} else {
				result->read += 4;
				uint32_t length = _cbor_load_uint32(source + 1);
				_cbor_handle_load_bytestring(source + 5, source_size - 5, length, res, result);
			}
			break;
		}
	case 0x5B:
		/* Bytestring with eight bytes length */
		{
			res->type = CBOR_TYPE_BYTESTRING;
			/* We have read one byte, need at least one more for the uint8_t */
			if (!_cbor_assert_avail_bytes(8, source_size - 1, result)) {
				result->error.position = 1;
			} else {
				result->read += 8;
				uint64_t length = _cbor_load_uint64(source + 1);
				_cbor_handle_load_bytestring(source + 9, source_size - 9, length, res, result);
			}
			break;
		}
	/* RESERVED - 0x5C - 0x5E */
	case 0x5F:
		/* Indefinite length bytestring */
		{
			/* Return "handle" object that will allow access to single chunks */
			res->type = CBOR_TYPE_BYTESTRING;
			res->data = NULL;
			// TODO metadata
			res->metadata.bytestring_metadata = (struct _cbor_bytestring_metadata) { 0, _CBOR_STRING_METADATA_INDEFINITE };
			break;
		}

	/* TODO - utf strings*/
	/* Arrays with embedded length */
	case 0x80:
	case 0x81:
	case 0x82:
	case 0x83:
	case 0x84:
	case 0x85:
	case 0x86:
	case 0x87:
	case 0x88:
	case 0x89:
	case 0x8A:
	case 0x8B:
	case 0x8C:
	case 0x8D:
	case 0x8E:
	case 0x8F:
	case 0x90:
	case 0x91:
	case 0x92:
	case 0x93:
	case 0x94:
	case 0x95:
	case 0x96:
	case 0x97:
		/* Array with embedded length */
		{
			uint8_t size = _cbor_load_uint8(source) - 0x80; /* Offset by 0x80 */
			res->type = CBOR_TYPE_ARRAY;
			res->data = malloc(sizeof(cbor_item_t *) * size);
			res->metadata.array_metadata = (struct _cbor_array_metadata) { (size_t) size, _CBOR_ARRAY_METADATA_DEFINITE };
			source++;
			source_size--;
			struct cbor_load_result subres;
			for (uint8_t i = 0; i < size; i++) {
				// TODO check subres
				// TODO extract this logic for usage with different arrays
				((cbor_item_t **)res->data)[i] = cbor_load(source, source_size, flags, &subres);
				source += subres.read;
				source_size -= subres.read;
				result->read += subres.read;
			}
			break;
		}
	case 0xFF:
		/* Indefinite length item break */
		{
			res->type = CBOR_TYPE_FLOAT_CTRL;
			res->metadata.float_ctrl_metadata = (struct _cbor_float_ctrl_metadata) { CBOR_FLOAT_0, CBOR_CTRL_BREAK };
			break;
		}
	default:
		{
		}
	}

	if (result->error.code != CBOR_ERR_NONE) {
		free(res);
		return NULL;
	} else
		return res;
}

bool _cbor_claim_bytes(size_t required, size_t provided, struct cbor_decoder_result * result)
{
	if (required > (provided - result->read)) {
		/* We need to keep all the metadata if parsing is to be resumed */
		result->read = 0;
		result->status = CBOR_DECODER_NEDATA;
		return false;
	} else {
		result->read += required;
		return true;
	}
}

struct cbor_decoder_result cbor_stream_decode(cbor_data source, size_t source_size, const struct cbor_callbacks * callbacks)
{
	/* If we have no data, we cannot read even the MTB */
	if (source_size < 1) {
		return (struct cbor_decoder_result){ 0, CBOR_DECODER_EBUFFER };
	}

	/* If we have a byte, assume it's the MTB */
	struct cbor_decoder_result result = { 1, CBOR_DECODER_FINISHED };

	switch(*source) {
	case 0x00: /* Fallthrough */
	case 0x01: /* Fallthrough */
	case 0x02: /* Fallthrough */
	case 0x03: /* Fallthrough */
	case 0x04: /* Fallthrough */
	case 0x05: /* Fallthrough */
	case 0x06: /* Fallthrough */
	case 0x07: /* Fallthrough */
	case 0x08: /* Fallthrough */
	case 0x09: /* Fallthrough */
	case 0x0A: /* Fallthrough */
	case 0x0B: /* Fallthrough */
	case 0x0C: /* Fallthrough */
	case 0x0D: /* Fallthrough */
	case 0x0E: /* Fallthrough */
	case 0x0F: /* Fallthrough */
	case 0x10: /* Fallthrough */
	case 0x11: /* Fallthrough */
	case 0x12: /* Fallthrough */
	case 0x13: /* Fallthrough */
	case 0x14: /* Fallthrough */
	case 0x15: /* Fallthrough */
	case 0x16: /* Fallthrough */
	case 0x17:
		/* Embedded one byte unsigned integer */
		{
			callbacks->uint8(_cbor_load_uint8(source));
			return result;
		}
	case 0x18:
		/* One byte unsigned integer */
		{
			if (_cbor_claim_bytes(1, source_size, &result)) {
				callbacks->uint8(_cbor_load_uint8(source + 1));
			}
			return result;
		}
	case 0x19:
		/* Two bytes unsigned integer */
		{
			if (_cbor_claim_bytes(2, source_size, &result)) {
				callbacks->uint16(_cbor_load_uint16(source + 1));
			}
			return result;
		}
	case 0x1A:
		/* Four bytes unsigned integer */
		{
			if (_cbor_claim_bytes(4, source_size, &result)) {
				callbacks->uint32(_cbor_load_uint32(source + 1));
			}
			return result;
		}
	case 0x1B:
		/* Eight bytes unsigned integer */
		{
			if (_cbor_claim_bytes(8, source_size, &result)) {
				callbacks->uint64(_cbor_load_uint64(source + 1));
			}
			return result;
		}
	case 0x1C: /* Fallthrough */
	case 0x1D: /* Fallthrough */
	case 0x1E: /* Fallthrough */
	case 0x1F:
		/* Reserved */
		{
			return (struct cbor_decoder_result){ 0, CBOR_DECODER_ERROR };
		}
	case 0x20: /* Fallthrough */
	case 0x21: /* Fallthrough */
	case 0x22: /* Fallthrough */
	case 0x23: /* Fallthrough */
	case 0x24: /* Fallthrough */
	case 0x25: /* Fallthrough */
	case 0x26: /* Fallthrough */
	case 0x27: /* Fallthrough */
	case 0x28: /* Fallthrough */
	case 0x29: /* Fallthrough */
	case 0x2A: /* Fallthrough */
	case 0x2B: /* Fallthrough */
	case 0x2C: /* Fallthrough */
	case 0x2D: /* Fallthrough */
	case 0x2E: /* Fallthrough */
	case 0x2F: /* Fallthrough */
	case 0x30: /* Fallthrough */
	case 0x31: /* Fallthrough */
	case 0x32: /* Fallthrough */
	case 0x33: /* Fallthrough */
	case 0x34: /* Fallthrough */
	case 0x35: /* Fallthrough */
	case 0x36: /* Fallthrough */
	case 0x37:
		/* Embedded one byte negative integer */
		{
			callbacks->negint8(_cbor_load_uint8(source) - 0x20); /* 0x20 offset */
			return result;
		}
	case 0x38:
		/* One byte negative integer */
	{
		if (_cbor_claim_bytes(1, source_size, &result)) {
			callbacks->negint8(_cbor_load_uint8(source + 1));
		}
		return result;
	}
	case 0x39:
		/* Two bytes negative integer */
	{
		if (_cbor_claim_bytes(2, source_size, &result)) {
			callbacks->negint16(_cbor_load_uint16(source + 1));
		}
		return result;
	}
	case 0x3A:
		/* Four bytes negative integer */
	{
		if (_cbor_claim_bytes(4, source_size, &result)) {
			callbacks->negint32(_cbor_load_uint32(source + 1));
		}
		return result;
	}
	case 0x3B:
		/* Eight bytes negative integer */
	{
		if (_cbor_claim_bytes(8, source_size, &result)) {
			callbacks->negint64(_cbor_load_uint64(source + 1));
		}
		return result;
	}
	case 0x3C: /* Fallthrough */
	case 0x3D: /* Fallthrough */
	case 0x3E: /* Fallthrough */
	case 0x3F:
		/* Reserved */
		{
			return (struct cbor_decoder_result){ 0, CBOR_DECODER_ERROR };
		}
	case 0x40: /* Fallthrough */
	case 0x41: /* Fallthrough */
	case 0x42: /* Fallthrough */
	case 0x43: /* Fallthrough */
	case 0x44: /* Fallthrough */
	case 0x45: /* Fallthrough */
	case 0x46: /* Fallthrough */
	case 0x47: /* Fallthrough */
	case 0x48: /* Fallthrough */
	case 0x49: /* Fallthrough */
	case 0x4A: /* Fallthrough */
	case 0x4B: /* Fallthrough */
	case 0x4C: /* Fallthrough */
	case 0x4D: /* Fallthrough */
	case 0x4E: /* Fallthrough */
	case 0x4F: /* Fallthrough */
	case 0x50: /* Fallthrough */
	case 0x51: /* Fallthrough */
	case 0x52: /* Fallthrough */
	case 0x53: /* Fallthrough */
	case 0x54: /* Fallthrough */
	case 0x55: /* Fallthrough */
	case 0x56: /* Fallthrough */
	case 0x57:
		/* Embedded length byte string */
		{
			size_t length = (size_t)_cbor_load_uint8(source) - 0x40; /* 0x40 offset */
			if (_cbor_claim_bytes(length, source_size, &result)) {
				callbacks->byte_string(source + 1, length);
			}
			return result;
		}
	case 0x58:
		/* One byte length byte string */
		// TODO template this?
		{
			if (_cbor_claim_bytes(1, source_size, &result)) {
				size_t length = (size_t) _cbor_load_uint8(source + 1);
				if (_cbor_claim_bytes(length, source_size, &result)) {
					callbacks->byte_string(source + 1 + 1, length);
				}
			}
			return result;
		}
	case 0x59:
		/* Two bytes length byte string */
		{
			if (_cbor_claim_bytes(2, source_size, &result)) {
				size_t length = (size_t) _cbor_load_uint16(source + 1);
				if (_cbor_claim_bytes(length, source_size, &result)) {
					callbacks->byte_string(source + 1 + 2, length);
				}
			}
			return result;
		}
	case 0x5A:
		/* Four bytes length byte string */
		{
			if (_cbor_claim_bytes(4, source_size, &result)) {
				size_t length = (size_t) _cbor_load_uint32(source + 1);
				if (_cbor_claim_bytes(length, source_size, &result)) {
					callbacks->byte_string(source + 1 + 4, length);
				}
			}
			return result;
		}
	case 0x5B:
		/* Eight bytes length byte string */
		{
			if (_cbor_claim_bytes(8, source_size, &result)) {
				size_t length = (size_t) _cbor_load_uint64(source + 1);
				if (_cbor_claim_bytes(length, source_size, &result)) {
					callbacks->byte_string(source + 1 + 8, length);
				}
			}
			return result;
		}
	case 0x5C: /* Fallthrough */
	case 0x5D: /* Fallthrough */
	case 0x5E:
		/* Reserved */
		{
			return (struct cbor_decoder_result){ 0, CBOR_DECODER_ERROR };
		}
	case 0x5F:
		/* Indefinite byte string */
		{
			callbacks->byte_string_start();
			return result;
		}
	case 0x60: /* Fallthrough */
	case 0x61: /* Fallthrough */
	case 0x62: /* Fallthrough */
	case 0x63: /* Fallthrough */
	case 0x64: /* Fallthrough */
	case 0x65: /* Fallthrough */
	case 0x66: /* Fallthrough */
	case 0x67: /* Fallthrough */
	case 0x68: /* Fallthrough */
	case 0x69: /* Fallthrough */
	case 0x6A: /* Fallthrough */
	case 0x6B: /* Fallthrough */
	case 0x6C: /* Fallthrough */
	case 0x6D: /* Fallthrough */
	case 0x6E: /* Fallthrough */
	case 0x6F: /* Fallthrough */
	case 0x70: /* Fallthrough */
	case 0x71: /* Fallthrough */
	case 0x72: /* Fallthrough */
	case 0x73: /* Fallthrough */
	case 0x74: /* Fallthrough */
	case 0x75: /* Fallthrough */
	case 0x76: /* Fallthrough */
	case 0x77:
		/* Embedded one byte length string */
		{
			// TODO
			return result;
		}
	case 0x78:
		/* One byte length string */
	case 0x79:
		/* Two bytes length string */
	case 0x7A:
		/* Four bytes length string */
	case 0x7B:
		/* Eight bytes length string */
	case 0x7C: /* Fallthrough */
	case 0x7D: /* Fallthrough */
	case 0x7E:
		/* Reserved */
		{
			return (struct cbor_decoder_result){ 0, CBOR_DECODER_ERROR };
		}
	case 0x7F:
		/* Indefinite length string */
	case 0x80: /* Fallthrough */
	case 0x81: /* Fallthrough */
	case 0x82: /* Fallthrough */
	case 0x83: /* Fallthrough */
	case 0x84: /* Fallthrough */
	case 0x85: /* Fallthrough */
	case 0x86: /* Fallthrough */
	case 0x87: /* Fallthrough */
	case 0x88: /* Fallthrough */
	case 0x89: /* Fallthrough */
	case 0x8A: /* Fallthrough */
	case 0x8B: /* Fallthrough */
	case 0x8C: /* Fallthrough */
	case 0x8D: /* Fallthrough */
	case 0x8E: /* Fallthrough */
	case 0x8F: /* Fallthrough */
	case 0x90: /* Fallthrough */
	case 0x91: /* Fallthrough */
	case 0x92: /* Fallthrough */
	case 0x93: /* Fallthrough */
	case 0x94: /* Fallthrough */
	case 0x95: /* Fallthrough */
	case 0x96: /* Fallthrough */
	case 0x97:
		/* Embedded one byte length array */
		{
			callbacks->array_start((size_t)_cbor_load_uint8(source) - 0x80); /* 0x40 offset */
			return result;
		}
	case 0x98:
		/* One byte length array */
		{
			if (_cbor_claim_bytes(1, source_size, &result)) {
				callbacks->array_start((size_t)_cbor_load_uint8(source + 1));
			}
			return result;
		}
	case 0x99:
		/* Two bytes length string */
		{
			if (_cbor_claim_bytes(2, source_size, &result)) {
				callbacks->array_start((size_t)_cbor_load_uint16(source + 1));
			}
			return result;
		}
	case 0x9A:
		/* Four bytes length string */
		{
			if (_cbor_claim_bytes(4, source_size, &result)) {
				callbacks->array_start((size_t)_cbor_load_uint32(source + 1));
			}
			return result;
		}
	case 0x9B:
		/* Eight bytes length string */
		{
			if (_cbor_claim_bytes(8, source_size, &result)) {
				callbacks->array_start((size_t)_cbor_load_uint64(source + 1));
			}
			return result;
		}
	case 0x9C: /* Fallthrough */
	case 0x9D: /* Fallthrough */
	case 0x9E:
		/* Reserved */
		{
			return (struct cbor_decoder_result){ 0, CBOR_DECODER_ERROR };
		}
	case 0x9F:
		/* Indefinite length array */
		{
			callbacks->indef_array_start();
			return result;
		}
	case 0xA0: /* Fallthrough */
	case 0xA1: /* Fallthrough */
	case 0xA2: /* Fallthrough */
	case 0xA3: /* Fallthrough */
	case 0xA4: /* Fallthrough */
	case 0xA5: /* Fallthrough */
	case 0xA6: /* Fallthrough */
	case 0xA7: /* Fallthrough */
	case 0xA8: /* Fallthrough */
	case 0xA9: /* Fallthrough */
	case 0xAA: /* Fallthrough */
	case 0xAB: /* Fallthrough */
	case 0xAC: /* Fallthrough */
	case 0xAD: /* Fallthrough */
	case 0xAE: /* Fallthrough */
	case 0xAF: /* Fallthrough */
	case 0xB0: /* Fallthrough */
	case 0xB1: /* Fallthrough */
	case 0xB2: /* Fallthrough */
	case 0xB3: /* Fallthrough */
	case 0xB4: /* Fallthrough */
	case 0xB5: /* Fallthrough */
	case 0xB6: /* Fallthrough */
	case 0xB7:
		/* Embedded one byte length map */
		{
			callbacks->map_start((size_t)_cbor_load_uint8(source) - 0xA0); /* 0xA0 offset */
			return result;
		}
	case 0xB8:
		/* One byte length map */
		{
			if (_cbor_claim_bytes(1, source_size, &result)) {
				callbacks->map_start((size_t)_cbor_load_uint8(source + 1));
			}
			return result;
		}
	case 0xB9:
		/* Two bytes length map */
		{
			if (_cbor_claim_bytes(2, source_size, &result)) {
				callbacks->map_start((size_t)_cbor_load_uint16(source + 1));
			}
			return result;
		}
	case 0xBA:
		/* Four bytes length map */
		{
			if (_cbor_claim_bytes(4, source_size, &result)) {
				callbacks->map_start((size_t)_cbor_load_uint32(source + 1));
			}
			return result;
		}
	case 0xBB:
		/* Eight bytes length map */
		{
			if (_cbor_claim_bytes(8, source_size, &result)) {
				callbacks->map_start((size_t)_cbor_load_uint64(source + 1));
			}
			return result;
		}
	case 0xBC: /* Fallthrough */
	case 0xBD: /* Fallthrough */
	case 0xBE:
		/* Reserved */
		{
			return (struct cbor_decoder_result){ 0, CBOR_DECODER_ERROR };
		}
	case 0xBF:
		/* Indefinite length map */
		{
			callbacks->indef_map_start();
			return result;
		}
	case 0xC0:
	case 0xC1:
	case 0xC2:
	case 0xC3:
	case 0xC4:
	case 0xC5:
	case 0xC6:
	case 0xC7:
	case 0xC8:
	case 0xC9:
	case 0xCA:
	case 0xCB:
	case 0xCC:
	case 0xCD:
	case 0xCE:
	case 0xCF:
	case 0xD0:
	case 0xD1:
	case 0xD2:
	case 0xD3:
	case 0xD4:
	case 0xD5:
	case 0xD6:
	case 0xD7:
	case 0xD8:
	case 0xD9:
	case 0xDA:
	case 0xDB:
	case 0xDC:
	case 0xDD:
	case 0xDE:
	case 0xDF:
	case 0xE0:
	case 0xE1:
	case 0xE2:
	case 0xE3:
	case 0xE4:
	case 0xE5:
	case 0xE6:
	case 0xE7:
	case 0xE8:
	case 0xE9:
	case 0xEA:
	case 0xEB:
	case 0xEC:
	case 0xED:
	case 0xEE:
	case 0xEF:
	case 0xF0:
	case 0xF1:
	case 0xF2:
	case 0xF3:
	case 0xF4:
	case 0xF5:
	case 0xF6:
	case 0xF7:
	case 0xF8:
	case 0xF9:
	case 0xFA:
	case 0xFB:
	case 0xFC:
	case 0xFD:
	case 0xFE:
	case 0xFF:
		/* Break */
		{
			callbacks->indef_break();
			return result;
		}
	default:
		{
			// TODO
			return result;
		}
	}
}


inline cbor_type cbor_typeof(const cbor_item_t * item)
{
	return item->type;
}


cbor_int_width cbor_int_get_width(const cbor_item_t * item)
{
	assert(cbor_is_int(item));
	return item->metadata.int_metadata.width;
}

uint8_t cbor_get_uint8(const cbor_item_t * item)
{
	return *(uint8_t *)item->data;
}

uint16_t cbor_get_uint16(const cbor_item_t * item)
{
	return *(uint16_t *)item->data;
}

uint32_t cbor_get_uint32(const cbor_item_t * item)
{
	return *(uint32_t *)item->data;
}

uint64_t cbor_get_uint64(const cbor_item_t * item)
{
	return *(uint64_t *)item->data;
}

void cbor_set_uint8(cbor_item_t * item, uint8_t value)
{
	assert(cbor_is_int(item));
	assert(cbor_int_get_width(item) == CBOR_INT_8);
	*(uint8_t *)item->data = value;
}

void cbor_set_uint16(cbor_item_t * item, uint16_t value)
{
	assert(cbor_is_int(item));
	assert(cbor_int_get_width(item) == CBOR_INT_16);
	*(uint16_t *)item->data = value;
}


void cbor_set_uint32(cbor_item_t * item, uint32_t value)
{
	assert(cbor_is_int(item));
	assert(cbor_int_get_width(item) == CBOR_INT_32);
	*(uint32_t *)item->data = value;
}

void cbor_set_uint64(cbor_item_t * item, uint64_t value)
{
	assert(cbor_is_int(item));
	assert(cbor_int_get_width(item) == CBOR_INT_64);
	*(uint64_t *)item->data = value;
}

void cbor_mark_uint(cbor_item_t * item)
{
	assert(cbor_is_int(item));
	item->type = CBOR_TYPE_UINT;
}

void cbor_mark_negint(cbor_item_t * item)
{
	assert(cbor_is_int(item));
	item->type = CBOR_TYPE_NEGINT;
}

cbor_item_t * cbor_new_int8()
{
	cbor_item_t * item = malloc(sizeof(cbor_item_t) + 1);
	item->data = (unsigned char *)item + sizeof(cbor_item_t);
	return item;
}

cbor_item_t * cbor_new_int16()
{
	cbor_item_t * item = malloc(sizeof(cbor_item_t) + 2);
	item->data = (unsigned char *)item + sizeof(cbor_item_t);
	return item;
}

cbor_item_t * cbor_new_int32()
{
	cbor_item_t * item = malloc(sizeof(cbor_item_t) + 4);
	item->data = (unsigned char *)item + sizeof(cbor_item_t);
	return item;
}

cbor_item_t * cbor_new_int64()
{
	cbor_item_t * item = malloc(sizeof(cbor_item_t) + 8);
	item->data = (unsigned char *)item + sizeof(cbor_item_t);
	return item;
}



/** ========================================================== */

inline bool cbor_isa_uint(const cbor_item_t * item)
{
	return item->type == CBOR_TYPE_UINT;
}

inline bool cbor_isa_negint(const cbor_item_t * item)
{
	return item->type == CBOR_TYPE_NEGINT;
}

inline bool cbor_isa_bytestring(const cbor_item_t * item)
{
	return item->type == CBOR_TYPE_BYTESTRING;
}

inline bool cbor_isa_string(const cbor_item_t * item)
{
	return item->type == CBOT_TYPE_STRING;
}

inline bool cbor_isa_array(const cbor_item_t * item)
{
	return item->type == CBOR_TYPE_ARRAY;
}

inline bool cbor_isa_map(const cbor_item_t * item)
{
	return item->type == CBOR_TYPE_MAP;
}

inline bool cbor_isa_tag(const cbor_item_t * item)
{
	return item->type == CBOR_TYPE_TAG;
}

inline bool cbor_isa_float_ctrl(const cbor_item_t * item)
{
	return item->type == CBOR_TYPE_FLOAT_CTRL;
}


/** ========================================================== */


inline bool cbor_is_int(const cbor_item_t * item)
{
	return cbor_isa_uint(item) || cbor_isa_negint(item);
}

inline bool cbor_is_uint(const cbor_item_t * item)
{
	/* Negative 'signed' ints are negints */
	return cbor_isa_uint(item);
}


inline bool cbor_is_bool(const cbor_item_t * item)
{
	return cbor_isa_float_ctrl(item) && (cbor_float_ctrl_get_ctrl(item) == CBOR_CTRL_FALSE || cbor_float_ctrl_get_ctrl(item) == CBOR_CTRL_TRUE);
}

inline bool cbor_is_null(const cbor_item_t * item)
{
	return cbor_isa_float_ctrl(item) && cbor_float_ctrl_get_ctrl(item) == CBOR_CTRL_NULL;
}

inline bool cbor_is_undef(const cbor_item_t * item)
{
	return cbor_isa_float_ctrl(item) && cbor_float_ctrl_get_ctrl(item) == CBOR_CTRL_UNDEF;
}

inline bool cbor_is_break(const cbor_item_t * item)
{
	return cbor_isa_float_ctrl(item) && cbor_float_ctrl_get_ctrl(item) == CBOR_CTRL_BREAK;
}

size_t cbor_bytestring_length(const cbor_item_t * item) {
	assert(cbor_isa_bytestring(item));
	return item->metadata.bytestring_metadata.length;
}

unsigned char * cbor_bytestring_handle(const cbor_item_t * item) {
	assert(cbor_isa_bytestring(item));
	return item->data;
}

bool cbor_bytestring_is_definite(const cbor_item_t * item)
{
	assert(cbor_isa_bytestring(item));
	return item->metadata.bytestring_metadata.type == _CBOR_STRING_METADATA_DEFINITE;
}

bool cbor_bytestring_is_indefinite(const cbor_item_t * item)
{
	return !cbor_bytestring_is_definite(item);
}

cbor_item_t * cbor_bytestring_get_chunk(const cbor_item_t * item)
{
	assert(cbor_isa_bytestring(item));
	assert(cbor_bytestring_is_indefinite(item));
	return (cbor_item_t *)item->data;
}

void cbor_bytestring_read_chunk(cbor_item_t * item, const unsigned char * source, size_t source_size, struct cbor_load_result * result)
{
	assert(cbor_isa_bytestring(item));
	assert(cbor_bytestring_is_indefinite(item));
	// TODO save original flags
	cbor_item_t * chunk = cbor_load(source, source_size, CBOR_FLAGS_NONE, result);
	/* Will be NULL if failed, no memory lost */
	item->data = (unsigned char *)chunk;
}

size_t cbor_array_get_size(cbor_item_t * item)
{
	assert(cbor_isa_array(item));
	return item->metadata.array_metadata.size;
}

bool cbor_array_is_definite(cbor_item_t * item)
{
	assert(cbor_isa_array(item));
	return item->metadata.array_metadata.type == _CBOR_ARRAY_METADATA_DEFINITE;
}

bool cbor_array_is_indefinite(cbor_item_t * item)
{
	assert(cbor_isa_array(item));
	return item->metadata.array_metadata.type == _CBOR_ARRAY_METADATA_INDEFINITE;
}

cbor_item_t ** cbor_array_handle(cbor_item_t * item)
{
	assert(cbor_isa_array(item));
	assert(cbor_array_is_definite(item));
	return (cbor_item_t **)item->data;
}

cbor_float_width cbor_float_ctrl_get_width(const cbor_item_t * item)
{
	assert(cbor_isa_float_ctrl(item));
	return item->metadata.float_ctrl_metadata.width;
}

cbor_ctrl cbor_float_ctrl_get_ctrl(const cbor_item_t * item)
{
	assert(cbor_isa_float_ctrl(item));
	assert(cbor_float_ctrl_get_width(item) == CBOR_FLOAT_0);
	return item->metadata.float_ctrl_metadata.type;
}

#ifdef DEBUG
void cbor_describe(cbor_item_t * item) {
	printf("Address: %p\t\t Type: ", (void *)item);
	switch(cbor_typeof(item)) {
	case CBOR_TYPE_UINT:
		{
			printf("UInt\n");
			break;
		}
	case CBOR_TYPE_NEGINT:
		{
			printf("NegInt\n");
			// TODO
			break;
		}
	case CBOR_TYPE_BYTESTRING:
		{
			printf("Bytestring\n");
			// TODO
			break;
		}
	case CBOT_TYPE_STRING:
		{
			printf("String\n");
			// TODO
			break;
		}
	case CBOR_TYPE_ARRAY:
		{
			printf("Array\n");
			// TODO
			break;
		}
	case CBOR_TYPE_MAP:
		{
			printf("Map\n");
			// TODO
			break;
		}
	case CBOR_TYPE_TAG:
		{
			printf("Tag\n");
			// TODO
			break;
		}
	case CBOR_TYPE_FLOAT_CTRL:
		{
			printf("Float/CTRL\n");
			// TODO
			break;
		}
	default:
		{
			printf("Invalid type! (%u)\n", cbor_typeof(item));
		}
	}
}

#endif
