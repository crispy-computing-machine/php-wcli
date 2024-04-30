
// ********************************************************************
// ************************ EXTENSION POUTINE *************************
// ********************************************************************


#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "php.h"
#include "ext/standard/info.h"
#include "php_wcli.h"
#include "wcli_arginfo.h"

/* For compatibility with older PHP versions */
#ifndef ZEND_PARSE_PARAMETERS_NONE
#define ZEND_PARSE_PARAMETERS_NONE() \
	ZEND_PARSE_PARAMETERS_START(0, 0) \
	ZEND_PARSE_PARAMETERS_END()
#endif

ZEND_DECLARE_MODULE_GLOBALS(wcli)

static void php_wcli_init_globals(zend_wcli_globals *wcli_globals) {}

static void flush_input_buffer();
static HWND get_console_window_handle();
static BOOL is_cmd_call();
static BOOL get_parent_proc(PROCESSENTRY32 *parent);
static DWORD get_parent_pid();
static HWND get_proc_window(DWORD pid);
static unsigned char get_key();
static unsigned char get_key_async();
static BOOL activate_window(HWND whnd);

PHP_RINIT_FUNCTION(wcli)
{
	WCLI_G(chnd) = GetStdHandle(STD_OUTPUT_HANDLE);
	if(WCLI_G(chnd) != NULL && WCLI_G(chnd) != INVALID_HANDLE_VALUE) WCLI_G(console) = TRUE;
	else WCLI_G(console) = FALSE;
	if(WCLI_G(console)) {
		WCLI_G(ihnd) = GetStdHandle(STD_INPUT_HANDLE);
		WCLI_G(parent).dwSize = 0;
		WCLI_G(whnd) = NULL;
		WCLI_G(cmdcall) = FALSE;
		WCLI_G(cmdcalli) = FALSE;
		GetConsoleScreenBufferInfo(WCLI_G(chnd), &WCLI_G(screen));
		GetConsoleCursorInfo(WCLI_G(chnd), &WCLI_G(cursor));
		GetCurrentConsoleFont(WCLI_G(chnd), FALSE, &WCLI_G(font));
		flush_input_buffer();
	}

#if defined(ZTS) && defined(COMPILE_DL_WCLI)
	ZEND_TSRMLS_CACHE_UPDATE();
#endif
	return SUCCESS;
}


PHP_MINFO_FUNCTION(wcli)
{
	php_info_print_table_start();
	php_info_print_table_row(2, "wcli support", "enabled");
	php_info_print_table_end();
}


PHP_MINIT_FUNCTION(wcli)
{
	// COLORS
	REGISTER_LONG_CONSTANT("Red",    FOREGROUND_RED, CONST_CS|CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("Green",  FOREGROUND_GREEN, CONST_CS|CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("Blue",   FOREGROUND_BLUE, CONST_CS|CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("Yellow", FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_INTENSITY, CONST_CS|CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("Purple", FOREGROUND_RED|FOREGROUND_BLUE, CONST_CS|CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("Aqua",   FOREGROUND_GREEN|FOREGROUND_BLUE, CONST_CS|CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("White",  FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_BLUE, CONST_CS|CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("Black",  0x00, CONST_CS|CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("Grey",   FOREGROUND_INTENSITY, CONST_CS|CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("Bright", FOREGROUND_INTENSITY, CONST_CS|CONST_PERSISTENT);

	// INIT GLOBALS
	ZEND_INIT_MODULE_GLOBALS(wcli, php_wcli_init_globals, NULL);
	return SUCCESS;
}


PHP_RSHUTDOWN_FUNCTION(wcli)
{
	if(WCLI_G(console)) {
		flush_input_buffer();
		SetConsoleTextAttribute(WCLI_G(chnd), WCLI_G(screen).wAttributes);
		SetConsoleCursorInfo(WCLI_G(chnd), &WCLI_G(cursor));
	}
	return SUCCESS;
}


zend_module_entry wcli_module_entry = {
	STANDARD_MODULE_HEADER,
	"wcli",                 /* Extension name */
	ext_functions,          /* zend_function_entry */
	PHP_MINIT(wcli),        /* PHP_MINIT - Module initialization */
	NULL,                   /* PHP_MSHUTDOWN - Module shutdown */
	PHP_RINIT(wcli),        /* PHP_RINIT - Request initialization */
	PHP_RSHUTDOWN(wcli),    /* PHP_RSHUTDOWN - Request shutdown */
	PHP_MINFO(wcli),        /* PHP_MINFO - Module info */
	PHP_WCLI_VERSION,       /* Version */
	STANDARD_MODULE_PROPERTIES
};


#ifdef COMPILE_DL_WCLI
# ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
# endif
ZEND_GET_MODULE(wcli)
#endif



// ********************************************************************
// ************************* HANDLE FUNCTIONS *************************
// ********************************************************************


ZEND_FUNCTION(wcli_get_output_handle)
{
	ZEND_PARSE_PARAMETERS_NONE();
	
	if(!WCLI_G(console)) RETURN_BOOL(FALSE);
	
	RETURN_LONG((zend_long)WCLI_G(chnd));
}


ZEND_FUNCTION(wcli_get_input_handle)
{
	ZEND_PARSE_PARAMETERS_NONE();
	
	if(!WCLI_G(console)) RETURN_BOOL(FALSE);
	
	RETURN_LONG((zend_long)WCLI_G(ihnd));
}


ZEND_FUNCTION(wcli_get_window_handle)
{
	ZEND_PARSE_PARAMETERS_NONE();
	
	if(!WCLI_G(console)) RETURN_BOOL(FALSE);
	
	RETURN_LONG((zend_long)get_console_window_handle());
}



// ********************************************************************
// ************************* CONSOLE FUNCTIONS ************************
// ********************************************************************


ZEND_FUNCTION(wcli_get_console_title)
{
	char title[512];

	ZEND_PARSE_PARAMETERS_NONE();

	if(!WCLI_G(console)) RETURN_BOOL(FALSE);
	if(!GetConsoleTitleA(title, 512)) RETURN_BOOL(FALSE);

	RETURN_STRING(title);
}


ZEND_FUNCTION(wcli_set_console_title)
{
	char *title;
	size_t size;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STRING(title, size)
	ZEND_PARSE_PARAMETERS_END();

	if(!WCLI_G(console)) RETURN_BOOL(FALSE);
	if(!SetConsoleTitle(title)) RETURN_BOOL(FALSE);

	RETURN_BOOL(TRUE);
}


ZEND_FUNCTION(wcli_get_console_size)
{
	HWND whnd;
	int sx,sy;
	CONSOLE_SCREEN_BUFFER_INFO info;

	ZEND_PARSE_PARAMETERS_NONE();

	if(!WCLI_G(console)) RETURN_BOOL(FALSE);
	if(!GetConsoleScreenBufferInfo(WCLI_G(chnd), &info)) RETURN_BOOL(FALSE);

	whnd = get_console_window_handle();
	sx = GetScrollPos(whnd, SB_HORZ);
	sy = GetScrollPos(whnd, SB_VERT);
	
	array_init(return_value);
	add_index_long(return_value, 0, info.srWindow.Right - info.srWindow.Left + 1);
	add_index_long(return_value, 1, info.srWindow.Bottom - info.srWindow.Top + 1);
	add_index_long(return_value, 2, sx);
	add_index_long(return_value, 3, sy);
}


ZEND_FUNCTION(wcli_set_console_size)
{
	CONSOLE_SCREEN_BUFFER_INFO info;
	zend_bool force = FALSE;
	SMALL_RECT size;
	zend_long w, h, bh;
	COORD bsize;
	COORD buff;

	ZEND_PARSE_PARAMETERS_START(2, 3)
		Z_PARAM_LONG(w)
		Z_PARAM_LONG(h)
		Z_PARAM_OPTIONAL
		Z_PARAM_BOOL(force)
	ZEND_PARSE_PARAMETERS_END();

	if(!WCLI_G(console)) RETURN_BOOL(FALSE);
	if(!GetConsoleScreenBufferInfo(WCLI_G(chnd), &info)) RETURN_BOOL(FALSE);

	bh = info.dwSize.Y;
	if(force) bh = h;

	buff.X = info.dwSize.X;
	buff.Y = info.dwSize.Y;
	if(w > info.dwSize.X) buff.X = w;
	if(h > info.dwSize.Y) buff.Y = h;

	if(buff.X != info.dwSize.X || buff.Y != info.dwSize.Y)
		if(!SetConsoleScreenBufferSize(WCLI_G(chnd), buff))
			RETURN_BOOL(FALSE);

	size.Top = 0;
	size.Left = 0;
	size.Right = w - 1;
	size.Bottom = h - 1;
	if(!SetConsoleWindowInfo(WCLI_G(chnd), TRUE, &size)) RETURN_BOOL(FALSE);

	bsize.X = w;
	bsize.Y = bh;
	if(!SetConsoleScreenBufferSize(WCLI_G(chnd), bsize)) RETURN_BOOL(FALSE);

	RETURN_BOOL(TRUE);
}


ZEND_FUNCTION(wcli_get_buffer_size)
{
	CONSOLE_SCREEN_BUFFER_INFO info;

	ZEND_PARSE_PARAMETERS_NONE();

	if(!WCLI_G(console)) RETURN_BOOL(FALSE);
	if(!GetConsoleScreenBufferInfo(WCLI_G(chnd), &info)) RETURN_BOOL(FALSE);
	
	array_init(return_value);
	add_index_long(return_value, 0, info.dwSize.X);
	add_index_long(return_value, 1, info.dwSize.Y);
}


ZEND_FUNCTION(wcli_set_buffer_size)
{
	zend_long w, h;
	COORD bsize;

	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_LONG(w)
		Z_PARAM_LONG(h)
	ZEND_PARSE_PARAMETERS_END();

	if(!WCLI_G(console)) RETURN_BOOL(FALSE);
	
	bsize.X = w;
	bsize.Y = h;
	if(!SetConsoleScreenBufferSize(WCLI_G(chnd), bsize)) RETURN_BOOL(FALSE);

	RETURN_BOOL(TRUE);
}


ZEND_FUNCTION(wcli_get_code_page)
{
	ZEND_PARSE_PARAMETERS_NONE();

	RETURN_LONG(GetConsoleCP());
}


ZEND_FUNCTION(wcli_set_code_page)
{
	zend_long cp;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_LONG(cp)
	ZEND_PARSE_PARAMETERS_END();

	RETURN_BOOL(SetConsoleCP(cp));
}


ZEND_FUNCTION(wcli_get_font_size)
{
	CONSOLE_FONT_INFO info;

	ZEND_PARSE_PARAMETERS_NONE();

	if(!WCLI_G(console)) RETURN_BOOL(0);
	GetCurrentConsoleFont(WCLI_G(chnd), FALSE, &info);
	
	array_init(return_value);
	add_index_long(return_value, 0, info.dwFontSize.X);
	add_index_long(return_value, 1, info.dwFontSize.Y);
}



// ********************************************************************
// ************************* COLORS FUNCTIONS *************************
// ********************************************************************


ZEND_FUNCTION(wcli_get_foreground_color)
{
	CONSOLE_SCREEN_BUFFER_INFO info;

	ZEND_PARSE_PARAMETERS_NONE();

	if(!WCLI_G(console)) RETURN_BOOL(FALSE);
	if(!GetConsoleScreenBufferInfo(WCLI_G(chnd), &info)) RETURN_BOOL(FALSE);
	
	RETURN_LONG(info.wAttributes & 0xF);
}


ZEND_FUNCTION(wcli_set_foreground_color)
{
	CONSOLE_SCREEN_BUFFER_INFO info;
	zend_long fore;
	
	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_LONG(fore)
	ZEND_PARSE_PARAMETERS_END();

	if(!WCLI_G(console)) RETURN_BOOL(FALSE);
	if(!GetConsoleScreenBufferInfo(WCLI_G(chnd), &info)) RETURN_BOOL(FALSE);
	if(!SetConsoleTextAttribute(WCLI_G(chnd), (info.wAttributes & 0xF0) | fore)) RETURN_BOOL(FALSE);
	
	RETURN_BOOL(true);
}


ZEND_FUNCTION(wcli_get_background_color)
{
	CONSOLE_SCREEN_BUFFER_INFO info;

	ZEND_PARSE_PARAMETERS_NONE();

	if(!WCLI_G(console)) RETURN_BOOL(FALSE);
	if(!GetConsoleScreenBufferInfo(WCLI_G(chnd), &info)) RETURN_BOOL(FALSE);
	
	RETURN_LONG(info.wAttributes >> 4);
}


ZEND_FUNCTION(wcli_set_background_color)
{
	CONSOLE_SCREEN_BUFFER_INFO info;
	zend_long back;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_LONG(back)
	ZEND_PARSE_PARAMETERS_END();
	
	if(!WCLI_G(console)) RETURN_BOOL(FALSE);
	if(!GetConsoleScreenBufferInfo(WCLI_G(chnd), &info)) RETURN_BOOL(FALSE);
	if(!SetConsoleTextAttribute(WCLI_G(chnd), (info.wAttributes & 0x0F) | (back << 4))) RETURN_BOOL(FALSE);

	RETURN_BOOL(TRUE);
}


ZEND_FUNCTION(wcli_get_colors)
{
	CONSOLE_SCREEN_BUFFER_INFO info;

	ZEND_PARSE_PARAMETERS_NONE();

	if(!WCLI_G(console)) RETURN_BOOL(FALSE);
	if(!GetConsoleScreenBufferInfo(WCLI_G(chnd), &info)) RETURN_BOOL(FALSE);

	array_init(return_value);
	add_index_long(return_value, 0, info.wAttributes & 0xF);
	add_index_long(return_value, 1, info.wAttributes >> 4);
}


ZEND_FUNCTION(wcli_set_colors)
{
	zend_long fore, back;

	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_LONG(fore)
		Z_PARAM_LONG(back)
	ZEND_PARSE_PARAMETERS_END();
	
	if(!WCLI_G(console)) RETURN_BOOL(FALSE);
	if(!SetConsoleTextAttribute(WCLI_G(chnd), (back << 4) | fore)) RETURN_BOOL(FALSE);
	
	RETURN_BOOL(TRUE);
}


ZEND_FUNCTION(wcli_inverse_colors)
{
	CONSOLE_SCREEN_BUFFER_INFO info;

	ZEND_PARSE_PARAMETERS_NONE();

	if(!WCLI_G(console)) RETURN_BOOL(FALSE);
	if(!GetConsoleScreenBufferInfo(WCLI_G(chnd), &info)) RETURN_BOOL(FALSE);
	if(!SetConsoleTextAttribute(WCLI_G(chnd), ((info.wAttributes & 0xF) << 4) | (info.wAttributes >> 4))) RETURN_BOOL(FALSE);

	RETURN_BOOL(TRUE);
}


ZEND_FUNCTION(wcli_reset_colors)
{
	ZEND_PARSE_PARAMETERS_NONE();

	if(!WCLI_G(console)) RETURN_BOOL(FALSE);
	if(!SetConsoleTextAttribute(WCLI_G(chnd), WCLI_G(screen).wAttributes)) RETURN_BOOL(FALSE);

	RETURN_BOOL(TRUE);
}



// ********************************************************************
// ************************* CURSOR FUNCTIONS *************************
// ********************************************************************


ZEND_FUNCTION(wcli_hide_cursor)
{
	CONSOLE_CURSOR_INFO info;

	ZEND_PARSE_PARAMETERS_NONE();

	if(!WCLI_G(console)) RETURN_BOOL(FALSE);
	if(!GetConsoleCursorInfo(WCLI_G(chnd), &info)) RETURN_BOOL(FALSE);
	
	info.bVisible = FALSE;
	if(!SetConsoleCursorInfo(WCLI_G(chnd), &info)) RETURN_BOOL(FALSE);
	
	RETURN_BOOL(TRUE);
}


ZEND_FUNCTION(wcli_show_cursor)
{
	CONSOLE_CURSOR_INFO info;
	
	ZEND_PARSE_PARAMETERS_NONE();

	if(!WCLI_G(console)) RETURN_BOOL(FALSE);
	if(!GetConsoleCursorInfo(WCLI_G(chnd), &info)) RETURN_BOOL(FALSE);
	
	info.bVisible = TRUE;
	if(!SetConsoleCursorInfo(WCLI_G(chnd), &info)) RETURN_BOOL(FALSE);
	
	RETURN_BOOL(TRUE);
}


ZEND_FUNCTION(wcli_get_cursor_visibility)
{
	CONSOLE_CURSOR_INFO info;

	ZEND_PARSE_PARAMETERS_NONE();

	if(!WCLI_G(console)) RETURN_BOOL(FALSE);
	if(!GetConsoleCursorInfo(WCLI_G(chnd), &info)) RETURN_BOOL(FALSE);

	RETURN_BOOL(info.bVisible);
}


ZEND_FUNCTION(wcli_set_cursor_visibility)
{
	CONSOLE_CURSOR_INFO info;
	zend_bool visible;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_BOOL(visible)
	ZEND_PARSE_PARAMETERS_END();

	if(!WCLI_G(console)) RETURN_BOOL(FALSE);
	if(!GetConsoleCursorInfo(WCLI_G(chnd), &info)) RETURN_BOOL(FALSE);
	
	info.bVisible = (BOOL)visible;
	if(!SetConsoleCursorInfo(WCLI_G(chnd), &info)) RETURN_BOOL(FALSE);
	
	RETURN_BOOL(TRUE);
}


ZEND_FUNCTION(wcli_get_cursor_size)
{
	CONSOLE_CURSOR_INFO info;
	
	ZEND_PARSE_PARAMETERS_NONE();

	if(!WCLI_G(console)) RETURN_BOOL(FALSE);
	if(!GetConsoleCursorInfo(WCLI_G(chnd), &info)) RETURN_BOOL(FALSE);
	
	RETURN_LONG(info.dwSize);
}


ZEND_FUNCTION(wcli_set_cursor_size)
{
	CONSOLE_CURSOR_INFO info;
	zend_long size;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_LONG(size)
	ZEND_PARSE_PARAMETERS_END();

	if(!WCLI_G(console)) RETURN_BOOL(FALSE);
	if(!GetConsoleCursorInfo(WCLI_G(chnd), &info)) RETURN_BOOL(FALSE);
	
	if(size < 1) size = 1;
	else if(size > 100) size = 100;
	info.dwSize = size;
	if(!SetConsoleCursorInfo(WCLI_G(chnd), &info)) RETURN_BOOL(FALSE);
	
	RETURN_BOOL(TRUE);
}


ZEND_FUNCTION(wcli_get_cursor_position)
{
	CONSOLE_SCREEN_BUFFER_INFO info;

	ZEND_PARSE_PARAMETERS_NONE();

	if(!WCLI_G(console)) RETURN_BOOL(FALSE);
	if(!GetConsoleScreenBufferInfo(WCLI_G(chnd), &info)) RETURN_BOOL(FALSE);
	
	array_init(return_value);
	add_index_long(return_value, 0, info.dwCursorPosition.X);
	add_index_long(return_value, 1, info.dwCursorPosition.Y);
}


ZEND_FUNCTION(wcli_set_cursor_position)
{
	COORD pos;
	zend_long x, y;
	CONSOLE_SCREEN_BUFFER_INFO info;

	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_LONG(x)
		Z_PARAM_LONG(y)
	ZEND_PARSE_PARAMETERS_END();

	if(!WCLI_G(console)) RETURN_BOOL(FALSE);
	if(!GetConsoleScreenBufferInfo(WCLI_G(chnd), &info)) RETURN_BOOL(FALSE);

	pos.X = x;
	pos.Y = y;
	if(pos.X < 0) pos.X = 0;
	if(pos.Y < 0) pos.Y = 0;
	if(!SetConsoleCursorPosition(WCLI_G(chnd), pos)) RETURN_BOOL(FALSE);

	RETURN_BOOL(TRUE);
}


ZEND_FUNCTION(wcli_move_cursor)
{
	COORD pos;
	zend_long x, y;
	CONSOLE_SCREEN_BUFFER_INFO info;

	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_LONG(x)
		Z_PARAM_LONG(y)
	ZEND_PARSE_PARAMETERS_END();

	if(!WCLI_G(console)) RETURN_BOOL(FALSE);
	if(!GetConsoleScreenBufferInfo(WCLI_G(chnd), &info)) RETURN_BOOL(FALSE);
	
	pos.X = x + info.dwCursorPosition.X;
	pos.Y = y + info.dwCursorPosition.Y;
	if(pos.X < 0) pos.X = 0;
	if(pos.Y < 0) pos.Y = 0;
	if(pos.X >= (info.srWindow.Right - info.srWindow.Left + 1)) pos.X = info.srWindow.Right - info.srWindow.Left;
	if(pos.Y >= (info.srWindow.Bottom - info.srWindow.Top + 1)) pos.Y = info.srWindow.Bottom - info.srWindow.Top;
	if(!SetConsoleCursorPosition(WCLI_G(chnd), pos)) RETURN_BOOL(FALSE);

	RETURN_BOOL(TRUE);
}



// ********************************************************************
// ************************* OUTPUT FUNCTIONS *************************
// ********************************************************************


ZEND_FUNCTION(wcli_echo)
{
	CONSOLE_SCREEN_BUFFER_INFO info;
	zend_long fore, back;
	zend_bool fore_isnull = 1, back_isnull = 1;
	char *str;
	size_t size;
	DWORD bytes;

	ZEND_PARSE_PARAMETERS_START(1, 3)
		Z_PARAM_STRING(str, size)
		Z_PARAM_OPTIONAL
		Z_PARAM_LONG_OR_NULL(fore, fore_isnull)
		Z_PARAM_LONG_OR_NULL(back, back_isnull)
	ZEND_PARSE_PARAMETERS_END();
	
	if(!WCLI_G(console)) RETURN_BOOL(FALSE);
	if(!GetConsoleScreenBufferInfo(WCLI_G(chnd), &info)) RETURN_BOOL(FALSE);
	
	if(fore_isnull) fore = info.wAttributes & 0xF;
	if(back_isnull) back = info.wAttributes >> 4;

	if(!SetConsoleTextAttribute(WCLI_G(chnd), (back << 4) | fore)) RETURN_BOOL(FALSE);
	if(!WriteConsole(WCLI_G(chnd), str, size, &bytes, NULL)) RETURN_BOOL(FALSE);
	if(!SetConsoleTextAttribute(WCLI_G(chnd), info.wAttributes)) RETURN_BOOL(FALSE);
	
	RETURN_BOOL(TRUE);
}


ZEND_FUNCTION(wcli_print)
{
	CONSOLE_SCREEN_BUFFER_INFO info;
	DWORD bytes;
	COORD pos;
	char *str;
	size_t size;
	zend_long x, y, fore, back;
	zend_bool x_isnull = 1, y_isnull = 1, fore_isnull = 1, back_isnull = 1;

	ZEND_PARSE_PARAMETERS_START(1, 5)
		Z_PARAM_STRING(str, size)
		Z_PARAM_OPTIONAL
		Z_PARAM_LONG_OR_NULL(x, x_isnull)
		Z_PARAM_LONG_OR_NULL(y, y_isnull)
		Z_PARAM_LONG_OR_NULL(fore, fore_isnull)
		Z_PARAM_LONG_OR_NULL(back, back_isnull)
	ZEND_PARSE_PARAMETERS_END();

	if(!WCLI_G(console)) RETURN_BOOL(FALSE);
	if(!GetConsoleScreenBufferInfo(WCLI_G(chnd), &info)) RETURN_BOOL(FALSE);

	if(x_isnull) x = info.dwCursorPosition.X;
	if(y_isnull) y = info.dwCursorPosition.Y;
	if(fore_isnull) fore = info.wAttributes & 0xF;
	if(back_isnull) back = info.wAttributes >> 4;

	pos.X = x;
	pos.Y = y;

	if(!FillConsoleOutputAttribute(WCLI_G(chnd), (back << 4) | fore, size, pos, &bytes))  RETURN_BOOL(FALSE);
	if(!WriteConsoleOutputCharacter(WCLI_G(chnd), str, size, pos, &bytes)) RETURN_BOOL(FALSE);

	RETURN_BOOL(TRUE);
}


ZEND_FUNCTION(wcli_clear)
{
	CONSOLE_SCREEN_BUFFER_INFO info;
	COORD pos = { .X = 0, .Y = 0 };
	DWORD nwc;

	ZEND_PARSE_PARAMETERS_NONE();

	if(!WCLI_G(console)) RETURN_BOOL(FALSE);
	if(!GetConsoleScreenBufferInfo(WCLI_G(chnd), &info)) RETURN_BOOL(FALSE);
	if(!FillConsoleOutputAttribute(WCLI_G(chnd), info.wAttributes, info.dwSize.X * info.dwSize.Y, pos, &nwc))  RETURN_BOOL(FALSE);
	if(!FillConsoleOutputCharacter(WCLI_G(chnd), 32, info.dwSize.X * info.dwSize.Y, pos, &nwc)) RETURN_BOOL(FALSE);
	if(!SetConsoleCursorPosition(WCLI_G(chnd), pos)) RETURN_BOOL(FALSE);

	RETURN_BOOL(TRUE);
}


ZEND_FUNCTION(wcli_fill)
{
	CONSOLE_SCREEN_BUFFER_INFO info;
	zend_long c, x, y, w, h, fore, back;
	zend_bool fore_isnull = 1, back_isnull = 1;
	WORD color;
	DWORD nwc;
	COORD pos;
	int i;

	ZEND_PARSE_PARAMETERS_START(5, 7)
		Z_PARAM_LONG(c)
		Z_PARAM_LONG(x)
		Z_PARAM_LONG(y)
		Z_PARAM_LONG(w)
		Z_PARAM_LONG(h)
		Z_PARAM_OPTIONAL
		Z_PARAM_LONG_OR_NULL(fore, fore_isnull)
		Z_PARAM_LONG_OR_NULL(back, back_isnull)
	ZEND_PARSE_PARAMETERS_END();

	if(!WCLI_G(console)) RETURN_BOOL(FALSE);
	if(!GetConsoleScreenBufferInfo(WCLI_G(chnd), &info)) RETURN_BOOL(FALSE);

	if(fore_isnull) fore = info.wAttributes & 0xF;
	if(back_isnull) back = info.wAttributes >> 4;

	if(c < 32 || c > 255) c = 35;
	if(x < 0 || y < 0 || w < 0 || h < 0) RETURN_BOOL(FALSE);
	if((x+w) > info.dwSize.X) w = info.dwSize.X - x;

	pos.X = x;
	color = (back << 4) | fore;

	for(i = 0; i < h && (i + y < info.dwSize.Y); i++) {
		pos.Y = y + i;
		if(!FillConsoleOutputAttribute(WCLI_G(chnd), color, w, pos, &nwc)) RETURN_BOOL(FALSE);
		if(!FillConsoleOutputCharacter(WCLI_G(chnd), c, w, pos, &nwc)) RETURN_BOOL(FALSE);
	}
	RETURN_BOOL(TRUE);
}



// ********************************************************************
// ************************* INPUT FUNCTIONS **************************
// ********************************************************************


ZEND_FUNCTION(wcli_get_key)
{
	unsigned char c;

	ZEND_PARSE_PARAMETERS_NONE();

	if(!WCLI_G(console)) RETURN_BOOL(FALSE);
	c = get_key();
	if(c == 0) RETURN_BOOL(FALSE);

	RETURN_LONG(c);
}


ZEND_FUNCTION(wcli_get_key_async)
{
	unsigned char c;

	ZEND_PARSE_PARAMETERS_NONE();

	if(!WCLI_G(console)) RETURN_BOOL(FALSE);
	c = get_key_async();
	if(c == 0) RETURN_BOOL(FALSE);
	
	RETURN_LONG(c);
}


ZEND_FUNCTION(wcli_flush_input_buffer)
{
	ZEND_PARSE_PARAMETERS_NONE();

	if(!WCLI_G(console)) RETURN_BOOL(FALSE);
	flush_input_buffer();

	RETURN_BOOL(TRUE);
}



// ********************************************************************
// ************************* WINDOW FUNCTIONS *************************
// ********************************************************************


ZEND_FUNCTION(wcli_is_on_top)
{
	ZEND_PARSE_PARAMETERS_NONE();

	if(!WCLI_G(console)) RETURN_BOOL(FALSE);
	if(get_console_window_handle() != GetForegroundWindow()) RETURN_BOOL(FALSE);
	
	RETURN_BOOL(TRUE);
}


ZEND_FUNCTION(wcli_is_visible)
{
	HWND whnd;

	ZEND_PARSE_PARAMETERS_NONE();

	if(!WCLI_G(console)) RETURN_BOOL(FALSE);
	whnd = get_console_window_handle();

	if(!whnd) RETURN_BOOL(FALSE);
	if(!IsWindowVisible(whnd)) RETURN_BOOL(FALSE);
	if(IsIconic(whnd)) RETURN_BOOL(FALSE);

	RETURN_BOOL(TRUE);
}


ZEND_FUNCTION(wcli_get_window_area)
{
	HWND whnd;
	RECT area;

	ZEND_PARSE_PARAMETERS_NONE();

	if(!WCLI_G(console)) RETURN_BOOL(FALSE);
	whnd = get_console_window_handle();
	
	if(!whnd) RETURN_BOOL(FALSE);
	if(!GetWindowRect(whnd, &area)) RETURN_BOOL(FALSE);

	array_init(return_value);
	add_index_long(return_value, 0, area.left);
	add_index_long(return_value, 1, area.top);
	add_index_long(return_value, 2, area.right - area.left);
	add_index_long(return_value, 3, area.bottom - area.top);
}


ZEND_FUNCTION(wcli_get_client_area)
{
	HWND whnd;
	RECT area;
	POINT pos;

	ZEND_PARSE_PARAMETERS_NONE();

	if(!WCLI_G(console)) RETURN_BOOL(FALSE);
	whnd = get_console_window_handle();

	if(!whnd) RETURN_BOOL(FALSE);
	if(!GetClientRect(whnd, &area)) RETURN_BOOL(FALSE);
	
	pos.x = area.left;
	pos.y = area.top;
	if(!ClientToScreen(whnd, &pos)) RETURN_BOOL(FALSE);

	array_init(return_value);
	add_index_long(return_value, 0, pos.x);
	add_index_long(return_value, 1, pos.y);
	add_index_long(return_value, 2, area.right - area.left);
	add_index_long(return_value, 3, area.bottom - area.top);
}


ZEND_FUNCTION(wcli_minimize)
{
	HWND whnd;

	ZEND_PARSE_PARAMETERS_NONE();

	if(!WCLI_G(console)) RETURN_BOOL(FALSE);
	whnd = get_console_window_handle();

	if(!whnd) RETURN_BOOL(FALSE);
	if(!IsWindowVisible(whnd)) RETURN_BOOL(FALSE);
	if(IsIconic(whnd)) RETURN_BOOL(TRUE);
	if(!ShowWindow(whnd, SW_MINIMIZE)) RETURN_BOOL(FALSE);

	RETURN_BOOL(TRUE);
}


ZEND_FUNCTION(wcli_maximize)
{
	HWND whnd;

	ZEND_PARSE_PARAMETERS_NONE();

	if(!WCLI_G(console)) RETURN_BOOL(FALSE);
	whnd = get_console_window_handle();

	if(!whnd) RETURN_BOOL(FALSE);
	if(!IsWindowVisible(whnd)) RETURN_BOOL(FALSE);
	if(!ShowWindow(whnd, SW_MAXIMIZE)) RETURN_BOOL(FALSE);
	
	RETURN_BOOL(TRUE);
}


ZEND_FUNCTION(wcli_restore)
{
	HWND whnd;

	ZEND_PARSE_PARAMETERS_NONE();

	if(!WCLI_G(console)) RETURN_BOOL(FALSE);
	whnd = get_console_window_handle();
	
	if(!whnd) RETURN_BOOL(FALSE);
	if(!IsWindowVisible(whnd)) RETURN_BOOL(FALSE);
	if(!ShowWindow(whnd, SW_RESTORE)) RETURN_BOOL(FALSE);

	RETURN_BOOL(TRUE);
}


ZEND_FUNCTION(wcli_activate)
{
	HWND whnd;

	ZEND_PARSE_PARAMETERS_NONE();

	if(!WCLI_G(console)) RETURN_BOOL(FALSE);
	whnd = get_console_window_handle();
	if(!whnd) RETURN_BOOL(FALSE);

	RETURN_BOOL(activate_window(whnd));
}


ZEND_FUNCTION(wcli_flash)
{
	zend_bool invert = FALSE;
	HWND whnd;

	ZEND_PARSE_PARAMETERS_START(0, 1)
		Z_PARAM_OPTIONAL
		Z_PARAM_BOOL(invert)
	ZEND_PARSE_PARAMETERS_END();

	if(!WCLI_G(console)) RETURN_BOOL(FALSE);
	whnd = get_console_window_handle();
	if(!whnd) RETURN_BOOL(FALSE);

	RETURN_BOOL(FlashWindow(whnd, invert));
}


ZEND_FUNCTION(wcli_bring_to_front)
{
	HWND whnd;

	ZEND_PARSE_PARAMETERS_NONE();

	if(!WCLI_G(console)) RETURN_BOOL(FALSE);
	whnd = get_console_window_handle();
	
	if(!whnd) RETURN_BOOL(FALSE);
	if(!IsWindowVisible(whnd)) RETURN_BOOL(FALSE);
	if(IsIconic(whnd) && !ShowWindow(whnd, SW_RESTORE)) RETURN_BOOL(FALSE);
	if(!SetWindowPos(whnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW)) RETURN_BOOL(FALSE);
	if(!SetWindowPos(whnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW)) RETURN_BOOL(FALSE);
	
	RETURN_BOOL(BringWindowToTop(whnd));
}


ZEND_FUNCTION(wcli_set_position)
{
	HWND whnd;
	zend_long x, y;

	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_LONG(x)
		Z_PARAM_LONG(y)
	ZEND_PARSE_PARAMETERS_END();

	if(!WCLI_G(console)) RETURN_BOOL(FALSE);
	whnd = get_console_window_handle();

	if(!whnd) RETURN_BOOL(FALSE);
	if(!activate_window(whnd)) RETURN_BOOL(FALSE);
	if(!SetWindowPos(whnd, HWND_TOP, x, y, 0, 0, SWP_NOSIZE)) RETURN_BOOL(FALSE);

	RETURN_BOOL(TRUE);
}



// ********************************************************************
// ************************** MISC FUNCTIONS **************************
// ********************************************************************


ZEND_FUNCTION(wcli_get_module_path)
{
	unsigned char path[4096];
	unsigned char *rpath;
	DWORD pathsize;

	ZEND_PARSE_PARAMETERS_NONE();

	pathsize = GetModuleFileName(NULL, path, 4096);
	rpath = emalloc(pathsize + 1);
	memcpy(rpath, path, pathsize);
	rpath[pathsize] = 0;
	
	RETURN_STRING(rpath);
}





// ********************************************************************
// *********************** INTERNAL FUNCTIONS *************************
// ********************************************************************


// Flush Input Console Buffer
static void flush_input_buffer()
{
	int i, k;
	for(k = 1; k;) {
		for(i = 1; i <= 256; i++) {
			if(k = GetAsyncKeyState(i) & 0x7FFF) {
				break;
			}
		}
	}
	FlushConsoleInputBuffer(WCLI_G(ihnd));
}


// Get Current Console Window Handle
static HWND get_console_window_handle()
{
	DWORD pid;

	// Lazy stuff
	if(WCLI_G(whnd) != NULL) return WCLI_G(whnd);

	// The rest of processssss...
	if(is_cmd_call()) pid = get_parent_pid();
	else pid = GetCurrentProcessId();
	WCLI_G(whnd) = get_proc_window(pid);
	return WCLI_G(whnd);
}


// Get if it is a command line call or an explorer call;
static BOOL is_cmd_call()
{
	PROCESSENTRY32 proc;

	// Lazy stuff
	if(WCLI_G(cmdcalli)) return WCLI_G(cmdcall);

	// Determine if CMD call or not
	if(!get_parent_proc(&proc)) return FALSE;
	if(_stricmp(proc.szExeFile, (const char *)"cmd.exe")) {
		WCLI_G(cmdcall) = FALSE;
		WCLI_G(cmdcalli) = TRUE;
		return FALSE;
	} else {
		WCLI_G(cmdcall) = TRUE;
		WCLI_G(cmdcalli) = TRUE;
		return TRUE;
	}
}


// Get Parent Process Info
static BOOL get_parent_proc(PROCESSENTRY32 *parent)
{
	HANDLE hsnap;
	BOOL ctn;
	DWORD pid;
	PROCESSENTRY32 proc;

	// Lazy stuff
	if(WCLI_G(parent).dwSize != 0) {
		memcpy(parent, &WCLI_G(parent), sizeof(PROCESSENTRY32));
		return TRUE;
	}

	// Get parent processID
	pid = GetCurrentProcessId();
	hsnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if(hsnap == INVALID_HANDLE_VALUE) return FALSE;
	proc.dwSize = sizeof(PROCESSENTRY32);
	for(ctn = Process32First(hsnap, &proc); ctn == TRUE; ctn = Process32Next(hsnap, &proc)) {
		if(proc.th32ProcessID == pid){
			break;
		}
	}

	// Get parent process info
	parent->dwSize = sizeof(PROCESSENTRY32);
	for(ctn = Process32First(hsnap, parent); ctn == TRUE; ctn = Process32Next(hsnap, parent)) {
		if(parent->th32ProcessID == proc.th32ParentProcessID) {
			CloseHandle(hsnap);
			memcpy(&WCLI_G(parent), parent, sizeof(PROCESSENTRY32));
			return TRUE;
		}
	}

	CloseHandle(hsnap);
	return FALSE;
}


// Get Parent Process PID
static DWORD get_parent_pid()
{
	PROCESSENTRY32 proc;
	if(!get_parent_proc(&proc)) return 0;
	else return proc.th32ProcessID;
}


// Get Process Main Window Handle
static HWND get_proc_window(DWORD pid)
{
	HWND whnd, parent, owner;
	DWORD wpid;

	for(whnd = FindWindow(NULL, NULL); whnd != NULL; whnd = GetWindow(whnd, GW_HWNDNEXT)) {
		parent = GetParent(whnd);
		GetWindowThreadProcessId(whnd, &wpid);
		if(wpid == pid && !parent) {
			owner = GetWindow(whnd, GW_OWNER);
			if(!owner && IsWindowVisible(whnd)) return whnd;
		}
	}

	return 0;
}


// Get Keyboard Key
static unsigned char get_key()
{
	unsigned int i;
	HWND whnd;

	whnd = get_console_window_handle();
	if(!whnd) return 0;

	for(flush_input_buffer();;) {
		if(whnd == GetForegroundWindow()) {
			for(i = 8; i <= 256; i++) {
				if(GetAsyncKeyState(i) & 0x7FFF) {
					FlushConsoleInputBuffer(WCLI_G(ihnd));
					return i;
				}
			}
		}
		Sleep(1);
	}
	return 0;
}


// Get Key Async
static unsigned char get_key_async()
{
	unsigned int i;
	HWND whnd;

	whnd = get_console_window_handle();
	if(whnd != GetForegroundWindow()) return 0;

	if(GetAsyncKeyState(VK_LBUTTON) & 0x8000) return VK_LBUTTON;
	for(i = 1; i <= 256; i++) {
		if(GetAsyncKeyState(i) & 0x7FFF) {
			FlushConsoleInputBuffer(WCLI_G(ihnd));
			return i;
		}
	}

	return 0;
}


// Activate a window
static BOOL activate_window(HWND whnd)
{
	if(!IsWindowVisible(whnd)) return FALSE;
	if(get_console_window_handle() == GetForegroundWindow()) return TRUE;
	if(!ShowWindow(whnd, SW_MINIMIZE)) return FALSE;
	if(!ShowWindow(whnd, SW_RESTORE)) return FALSE;
	if(!SetWindowPos(whnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE)) return FALSE;
	if(!BringWindowToTop(whnd)) return FALSE;
	return TRUE;
}