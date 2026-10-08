// SPDX-License-Identifier: GPL-2.0
/*
 * libnvram.so substituto para rodar o nas (autenticador WPA da Broadcom)
 * e as libs do DD-WRT no OpenWrt.  O libnvram original fala com o driver
 * /dev/nvram do kernel do DD-WRT; este le "chave=valor" de um arquivo
 * (WL_NVRAM ou /etc/wl-nvram.conf) e guarda as alteracoes so em memoria.
 *
 * Exporta toda a API que nas, libshutils, libutils e libwireless importam
 * (o musl resolve todos os simbolos no carregamento).
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct var {
	struct var *next;
	char *name;
	char *value;
};

static struct var *vars;
static int loaded;

static struct var *find(const char *name)
{
	struct var *v;

	for (v = vars; v; v = v->next)
		if (!strcmp(v->name, name))
			return v;
	return NULL;
}

static int put(const char *name, const char *value)
{
	struct var *v = find(name);
	char *nv = strdup(value);

	if (!nv)
		return -1;
	if (v) {
		free(v->value);
		v->value = nv;
		return 0;
	}
	v = calloc(1, sizeof(*v));
	if (!v || !(v->name = strdup(name))) {
		free(v);
		free(nv);
		return -1;
	}
	v->value = nv;
	v->next = vars;
	vars = v;
	return 0;
}

static void load(void)
{
	const char *path = getenv("WL_NVRAM");
	char line[1024], *eq, *end;
	FILE *f;

	if (loaded)
		return;
	loaded = 1;
	f = fopen(path ? path : "/etc/wl-nvram.conf", "r");
	if (!f)
		return;
	while (fgets(line, sizeof(line), f)) {
		if (line[0] == '#' || !(eq = strchr(line, '=')))
			continue;
		*eq = '\0';
		end = eq + 1 + strcspn(eq + 1, "\r\n");
		*end = '\0';
		put(line, eq + 1);
	}
	fclose(f);
}

static char *vfmt(char *buf, size_t len, const char *fmt, va_list ap)
{
	vsnprintf(buf, len, fmt, ap);
	return buf;
}

char *nvram_get(const char *name)
{
	struct var *v;

	if (!name)
		return NULL;
	load();
	v = find(name);
	return v ? v->value : NULL;
}

char *nvram_safe_get(const char *name)
{
	char *v = nvram_get(name);

	return v ? v : "";
}

int nvram_set(const char *name, const char *value)
{
	load();
	return name ? put(name, value ? value : "") : -1;
}

int nvram_unset(const char *name)
{
	struct var **pp, *v;

	load();
	for (pp = &vars; (v = *pp); pp = &v->next) {
		if (!strcmp(v->name, name)) {
			*pp = v->next;
			free(v->name);
			free(v->value);
			free(v);
			return 0;
		}
	}
	return 0;
}

int _nvram_commit(void)
{
	return 0;
}

int nvram_exists(const char *name)
{
	return nvram_get(name) != NULL;
}

int nvram_match(const char *name, const char *match)
{
	const char *v = nvram_get(name);

	return v && match && !strcmp(v, match);
}

int nvram_invmatch(const char *name, const char *invmatch)
{
	const char *v = nvram_get(name);

	return v && invmatch && strcmp(v, invmatch);
}

int nvram_geti(const char *name)
{
	return atoi(nvram_safe_get(name));
}

int nvram_matchi(const char *name, int match)
{
	const char *v = nvram_get(name);

	return v && atoi(v) == match;
}

int nvram_seti(const char *name, int value)
{
	char buf[16];

	snprintf(buf, sizeof(buf), "%d", value);
	return nvram_set(name, buf);
}

char *nvram_default_get(const char *name, const char *def)
{
	char *v = nvram_get(name);

	if (v && *v)
		return v;
	nvram_set(name, def);
	return nvram_get(name);
}

int nvram_default_geti(const char *name, int def)
{
	char buf[16];

	snprintf(buf, sizeof(buf), "%d", def);
	return atoi(nvram_default_get(name, buf));
}

char *nvram_nget(const char *fmt, ...)
{
	char name[256];
	va_list ap;

	va_start(ap, fmt);
	vfmt(name, sizeof(name), fmt, ap);
	va_end(ap);
	return nvram_safe_get(name);
}

int nvram_ngeti(const char *fmt, ...)
{
	char name[256];
	va_list ap;

	va_start(ap, fmt);
	vfmt(name, sizeof(name), fmt, ap);
	va_end(ap);
	return nvram_geti(name);
}

int nvram_nmatch(const char *match, const char *fmt, ...)
{
	char name[256];
	va_list ap;

	va_start(ap, fmt);
	vfmt(name, sizeof(name), fmt, ap);
	va_end(ap);
	return nvram_match(name, match);
}

int nvram_nmatchi(int match, const char *fmt, ...)
{
	char name[256];
	va_list ap;

	va_start(ap, fmt);
	vfmt(name, sizeof(name), fmt, ap);
	va_end(ap);
	return nvram_matchi(name, match);
}

int nvram_nset(const char *value, const char *fmt, ...)
{
	char name[256];
	va_list ap;

	va_start(ap, fmt);
	vfmt(name, sizeof(name), fmt, ap);
	va_end(ap);
	return nvram_set(name, value);
}

int nvram_nseti(int value, const char *fmt, ...)
{
	char name[256];
	va_list ap;

	va_start(ap, fmt);
	vfmt(name, sizeof(name), fmt, ap);
	va_end(ap);
	return nvram_seti(name, value);
}

int nvram_default_ngeti(int def, const char *fmt, ...)
{
	char name[256];
	va_list ap;

	va_start(ap, fmt);
	vfmt(name, sizeof(name), fmt, ap);
	va_end(ap);
	return nvram_default_geti(name, def);
}
