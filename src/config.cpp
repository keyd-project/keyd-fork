/*
 * keyd - A key remapping daemon.
 *
 * © 2019 Raheman Vaiya (see also: LICENSE).
 */

#include <assert.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <libgen.h>

#include "keyd.h"
#include "ini.h"
#include "keys.h"
#include "log.h"
#include "string.h"
#include "unicode.h"
#include <limits>
#include <string>
#include <string_view>
#include <iostream>
#include <fstream>

#ifndef DATA_DIR
#define DATA_DIR
#endif

#undef warn
#define warn(fmt, ...) keyd_log("\ty{WARNING:} " fmt "\n", ##__VA_ARGS__)

enum action_arg_e {
	ARG_EMPTY,

	ARG_MACRO,
	ARG_LAYER,
	ARG_LAYOUT,
	ARG_TIMEOUT,
	ARG_SENSITIVITY,
	ARG_DESCRIPTOR,
};

static struct {
	const char *name;
	const char *preferred_name;
	enum op op;
	enum action_arg_e args[MAX_DESCRIPTOR_ARGS];
} actions[] =  {
	{ "swap", 	NULL,	OP_SWAP,	{ ARG_LAYER } },
	{ "clear", 	NULL,	OP_CLEAR,	{} },
	{ "oneshot", 	NULL,	OP_ONESHOT,	{ ARG_LAYER } },
	{ "toggle", 	NULL,	OP_TOGGLE,	{ ARG_LAYER } },

	{ "clearm", 	NULL,	OP_CLEARM,	{ ARG_MACRO } },
	{ "swapm", 	NULL,	OP_SWAPM,	{ ARG_LAYER, ARG_MACRO } },
	{ "togglem", 	NULL,	OP_TOGGLEM,	{ ARG_LAYER, ARG_MACRO } },
	{ "layerm", 	NULL,	OP_LAYERM,	{ ARG_LAYER, ARG_MACRO } },
	{ "oneshotm", 	NULL,	OP_ONESHOTM,	{ ARG_LAYER, ARG_MACRO } },

	{ "layer", 	NULL,	OP_LAYER,	{ ARG_LAYER } },

	{ "overload", 	NULL,	OP_OVERLOAD,			{ ARG_LAYER, ARG_DESCRIPTOR } },
	{ "overloadt", 	NULL,	OP_OVERLOAD_TIMEOUT,		{ ARG_LAYER, ARG_DESCRIPTOR, ARG_TIMEOUT } },
	{ "overloadt2", NULL,	OP_OVERLOAD_TIMEOUT_TAP,	{ ARG_LAYER, ARG_DESCRIPTOR, ARG_TIMEOUT } },

	{ "overloadi",	NULL,	OP_OVERLOAD_IDLE_TIMEOUT, { ARG_DESCRIPTOR, ARG_DESCRIPTOR, ARG_TIMEOUT } },
	{ "timeout", 	NULL,	OP_TIMEOUT,	{ ARG_DESCRIPTOR, ARG_TIMEOUT, ARG_DESCRIPTOR } },

	{ "macro2", 	NULL,	OP_MACRO2,	{ ARG_TIMEOUT, ARG_TIMEOUT, ARG_MACRO } },
	{ "setlayout", 	NULL,	OP_LAYOUT,	{ ARG_LAYOUT } },

	/* Experimental */
	{ "scrollt", 	NULL,	OP_SCROLL_TOGGLE,		{ARG_SENSITIVITY} },
	{ "scroll", 	NULL,	OP_SCROLL,			{ARG_SENSITIVITY} },

	/* TODO: deprecate */
	{ "overload2", 	"overloadt",	OP_OVERLOAD_TIMEOUT,		{ ARG_LAYER, ARG_DESCRIPTOR, ARG_TIMEOUT } },
	{ "overload3", 	"overloadt2",	OP_OVERLOAD_TIMEOUT_TAP,	{ ARG_LAYER, ARG_DESCRIPTOR, ARG_TIMEOUT } },
	{ "toggle2", 	"togglem",	OP_TOGGLEM,			{ ARG_LAYER, ARG_MACRO } },
	{ "swap2", 	"swapm",	OP_SWAPM,			{ ARG_LAYER, ARG_MACRO } },
};

int config_get_layer_index(const struct config *config, std::string_view name);

static std::string resolve_include_path(const char *path, std::string_view include_path)
{
	std::string resolved_path;
	std::string tmp;

	if (include_path.ends_with(".conf")) {
		warn("%s: included file has invalid extension", include_path.data());
		return {};
	}

	tmp = path;
	resolved_path = dirname(tmp.data());
	resolved_path += '/';
	resolved_path += include_path;

	if (!access(resolved_path.c_str(), F_OK))
		return resolved_path.c_str();

	resolved_path = DATA_DIR "/";
	resolved_path += include_path;

	if (!access(resolved_path.c_str(), F_OK))
		return resolved_path.c_str();

	return resolved_path;
}

static std::string read_file(const char *path)
{
	constexpr std::string_view include_prefix = "include ";

	std::string buf, line;

	std::ifstream file(path);
	if (!file.is_open()) {
		err("failed to open %s", path);
		return {};
	}

	while (std::getline(file, line)) {
		if (line.starts_with(include_prefix)) {
			std::string_view include_path = line;
			include_path.remove_prefix(include_prefix.size());
			while (include_path.starts_with(' '))
				include_path.remove_prefix(1);

			auto resolved_path = resolve_include_path(path, include_path);
			if (resolved_path.empty()) {
				warn("failed to resolve include path: %s", include_path.data());
				continue;
			}

			std::ifstream file(resolved_path);
			if (!file.is_open()) {
				warn("failed to %s", line.c_str());
				perror("open");
			} else {
				buf.append(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
			}
		} else {
			buf += line;
			buf += '\n';
		}
	}

	return buf;
}


/* Return up to two keycodes associated with the given name. */
static uint8_t lookup_keycode(const char *name)
{
	size_t i;

	for (i = 0; i < 256; i++) {
		const struct keycode_table_ent *ent = &keycode_table[i];

		if (ent->name &&
		    (!strcmp(ent->name, name) ||
		     (ent->alt_name && !strcmp(ent->alt_name, name)))) {
			return i;
		}
	}

	return 0;
}

static struct descriptor *layer_lookup_chord(struct layer *layer, decltype(chord::keys)& keys, size_t n)
{
	for (size_t i = 0; i < layer->chords.size(); i++) {
		size_t nm = 0;
		struct chord *chord = &layer->chords[i];

		for (size_t j = 0; j < n; j++) {
			for (size_t k = 0; k < chord->keys.size(); k++)
				if (keys[j] == chord->keys[k]) {
					nm++;
					break;
				}
		}

		if (nm == n)
			return &chord->d;
	}

	return NULL;
}

/*
 * Consumes a string of the form `[<layer>.]<key> = <descriptor>` and adds the
 * mapping to the corresponding layer in the config.
 */

static int set_layer_entry(const struct config *config,
			   struct layer *layer, char *key,
			   const struct descriptor *d)
{
	int found = 0;

	if (strchr(key, '+')) {
		//TODO: Handle aliases
		char *tok;
		struct descriptor *ld;
		decltype(chord::keys) keys{};
		size_t n = 0;

		for (tok = strtok(key, "+"); tok; tok = strtok(NULL, "+")) {
			uint8_t code = lookup_keycode(tok);
			if (!code) {
				err("%s is not a valid key", tok);
				return -1;
			}

			if (n >= ARRAY_SIZE(keys)) {
				err("chords cannot contain more than %ld keys", n);
				return -1;
			}

			keys[n++] = code;
		}


		if ((ld = layer_lookup_chord(layer, keys, n))) {
			*ld = *d;
		} else {
			layer->chords.emplace_back() = {keys, *d};
		}
	} else {
		for (size_t i = 0; i < config->aliases.size(); i++) {
			if (config->aliases[i] == key) {
				layer->keymap[i] = *d;
				found = 1;
			}
		}

		if (!found) {
			uint8_t code;

			if (!(code = lookup_keycode(key))) {
				err("%s is not a valid key or alias", key);
				return -1;
			}

			layer->keymap[code] = *d;

		}
	}

	return 0;
}

static int new_layer(std::string_view s, const struct config *config, struct layer *layer)
{
	uint8_t mods;
	std::string_view name, type;

	name = s.substr(0, s.find_first_of(":"));
	if (name != s)
		type = s.substr(name.size() + 1);

	layer->name = name;
	layer->chords.clear();

	if (name.find_first_of("+") + 1 /* Found */) {
		int n = 0;

		layer->type = LT_COMPOSITE;
		layer->nr_constituents = 0;

		if (!type.empty()) {
			err("composite layers cannot have a type.");
			return -1;
		}

		while (true) {
			std::string_view layername = name.substr(0, s.find_first_of("+"));
			int idx = config_get_layer_index(config, layername);

			if (idx < 0) {
				err("%s is not a valid layer", std::string(layername).c_str());
				return -1;
			}

			if (n >= ARRAY_SIZE(layer->constituents)) {
				err("max composite layers (%d) exceeded", ARRAY_SIZE(layer->constituents));
				return -1;
			}

			layer->constituents[layer->nr_constituents++] = idx;

			if (name == layername)
				break;
			name.remove_prefix(layername.size() + 1);
		}

	} else if (!type.empty() && type == "layout") {
			layer->type = LT_LAYOUT;
	} else if (!type.empty() && !parse_modset(type.data() /* Must be NTS */, &mods)) {
			layer->type = LT_NORMAL;
			layer->mods = mods;
	} else {
		if (!type.empty())
			warn("\"%s\" is not a valid layer type, ignoring\n", type);

		layer->type = LT_NORMAL;
		layer->mods = 0;
	}


	return 0;
}

/*
 * Returns:
 * 	1 if the layer exists
 * 	0 if the layer was created successfully
 * 	< 0 on error
 */
static int config_add_layer(struct config *config, std::string_view name)
{
	int ret;

	if (config_get_layer_index(config, name.substr(0, name.find_first_of(":"))) != -1)
		return 1;

	::layer new_layer = {};
	ret = ::new_layer(name, config, &new_layer);

	if (ret < 0)
		return -1;

	config->layers.emplace_back(std::move(new_layer));
	return 0;
}

/* Modifies the input string */
static int parse_fn(char *s,
		    char **name,
		    char *args[5],
		    size_t *nargs)
{
	char *c, *arg;

	c = s;
	while (*c && *c != '(')
		c++;

	if (!*c)
		return -1;

	*name = s;
	*c++ = 0;

	while (*c == ' ')
		c++;

	*nargs = 0;
	arg = c;
	while (1) {
		int plvl = 0;

		while (*c) {
			switch (*c) {
			case '\\':
				if (*(c+1)) {
					c+=2;
					continue;
				}
				break;
			case '(':
				plvl++;
				break;
			case ')':
				plvl--;

				if (plvl == -1)
					goto exit;
				break;
			case ',':
				if (plvl == 0)
					goto exit;
				break;
			}

			c++;
		}
exit:

		if (!*c)
			return -1;

		if (arg != c) {
			assert(*nargs < 5);
			args[(*nargs)++] = arg;
		}

		if (*c == ')') {
			*c = 0;
			return 0;
		}

		*c++ = 0;
		while (*c == ' ')
			c++;
		arg = c;
	}
}

/*
 * Parses macro expression placing the result
 * in the supplied macro struct.
 *
 * Returns:
 *   0 on success
 *   -1 in the case of an invalid macro expression
 *   > 0 for all other errors
 */

int parse_macro_expression(const char *s, macro& macro)
{
	uint8_t code, mods;

	std::string buf = s;
	auto ptr = buf.data();

	if (buf.starts_with("macro(") && buf.ends_with(')')) {
		buf.pop_back();
		ptr += 6;
		str_escape(ptr);
	} else if (parse_key_sequence(ptr, &code, &mods) && utf8_strlen(ptr) != 1) {
		err("Invalid macro");
		return -1;
	}

	return macro_parse(ptr, macro) == 0 ? 0 : 1;
}

static int parse_command(const char *s, std::string& command)
{
	size_t len = strlen(s);

	if (len == 0 || strstr(s, "command(") != s || s[len-1] != ')')
		return -1;

	command = s + 8;
	command.pop_back(); // Remove )
	command.resize(str_escape(command.data()));

	return 0;
}

static int parse_descriptor(char *s,
			    struct descriptor *d,
			    struct config *config)
{
	char *fn = NULL;
	char *args[5];
	size_t nargs = 0;
	uint8_t code, mods;
	int ret;
	::macro macro;
	std::string cmd;

	if (!s || !s[0]) {
		d->op = OP_NULL;
		return 0;
	}

	if (!parse_key_sequence(s, &code, &mods)) {
		size_t i;
		const char *layer = NULL;

		switch (code) {
			case KEYD_LEFTSHIFT:   layer = "shift"; break;
			case KEYD_LEFTCTRL:    layer = "control"; break;
			case KEYD_LEFTMETA:    layer = "meta"; break;
			case KEYD_LEFTALT:     layer = "alt"; break;
			case KEYD_RIGHTALT:    layer = "altgr"; break;
		}

		if (layer) {
			warn("You should use b{layer(%s)} instead of assigning to b{%s} directly.", layer, KEY_NAME(code));
			d->op = OP_LAYER;
			d->args[0].idx = config_get_layer_index(config, layer);

			assert(d->args[0].idx != -1);

			return 0;
		}

		d->op = OP_KEYSEQUENCE;
		d->args[0].code = code;
		d->args[1].mods = mods;

		return 0;
	} else if ((ret = parse_command(s, cmd)) >= 0) {
		if (ret) {
			return -1;
		}

		if (config->commands.size() >= std::numeric_limits<decltype(d->args[0].idx)>::max()) {
			err("max commands exceeded");
			return -1;
		}

		d->op = OP_COMMAND;
		d->args[0].idx = config->commands.size();
		config->commands.emplace_back(std::move(cmd));

		return 0;
	} else if ((ret = parse_macro_expression(s, macro)) >= 0) {
		if (ret)
			return -1;

		if (config->macros.size() >= std::numeric_limits<decltype(d->args[0].idx)>::max()) {
			err("max macros exceeded");
			return -1;
		}

		d->op = OP_MACRO;
		d->args[0].idx = config->macros.size();
		config->macros.emplace_back(std::move(macro));

		return 0;
	} else if (!parse_fn(s, &fn, args, &nargs)) {
		int i;

		if (!strcmp(fn, "lettermod")) {
			std::string buf;

			if (nargs != 4) {
				err("%s requires 4 arguments", fn);
				return -1;
			}

			buf = buf + "overloadi(" + args[1] + ", overloadt2(" + args[0] + ", " + args[1] + ", " + args[3] + "), " + args[2] + ")";
			if (parse_fn(buf.data(), &fn, args, &nargs)) {
				err("failed to parse %s", buf.c_str());
				return -1;
			}
		}

		for (i = 0; i < ARRAY_SIZE(actions); i++) {
			if (!strcmp(actions[i].name, fn)) {
				int j;

				if (actions[i].preferred_name)
					warn("%s is deprecated (renamed to %s).", actions[i].name, actions[i].preferred_name);

				d->op = actions[i].op;

				for (j = 0; j < MAX_DESCRIPTOR_ARGS; j++) {
					if (!actions[i].args[j])
						break;
				}

				if ((int)nargs != j) {
					err("%s requires %d %s", actions[i].name, j, j == 1 ? "argument" : "arguments");
					return -1;
				}

				while (j--) {
					int type = actions[i].args[j];
					union descriptor_arg *arg = &d->args[j];
					char *argstr = args[j];
					struct descriptor desc;

					switch (type) {
					case ARG_LAYER:
						if (!strcmp(argstr, "main")) {
							err("the main layer cannot be toggled");
							return -1;
						}

						arg->idx = config_get_layer_index(config, argstr);
						if (arg->idx == -1 || config->layers[arg->idx].type == LT_LAYOUT) {
							err("%s is not a valid layer", argstr);
							return -1;
						}

						break;
					case ARG_LAYOUT:
						arg->idx = config_get_layer_index(config, argstr);
						if (arg->idx == -1 ||
							(arg->idx != 0 && //Treat main as a valid layout
							 config->layers[arg->idx].type != LT_LAYOUT)) {
							err("%s is not a valid layout", argstr);
							return -1;
						}

						break;
					case ARG_DESCRIPTOR:
						if (parse_descriptor(argstr, &desc, config))
							return -1;

						if (config->descriptors.size() >= std::numeric_limits<decltype(arg->idx)>::max()) {
							err("maximum descriptors exceeded");
							return -1;
						}

						arg->idx = config->descriptors.size();
						config->descriptors.emplace_back(std::move(desc));
						break;
					case ARG_SENSITIVITY:
						arg->sensitivity = atoi(argstr);
						break;
					case ARG_TIMEOUT:
						arg->timeout = atoi(argstr);
						break;
					case ARG_MACRO:
						if (config->macros.size() >= std::numeric_limits<decltype(arg->idx)>::max()) {
							err("max macros exceeded");
							return -1;
						}

						config->macros.emplace_back();
						if (parse_macro_expression(argstr, config->macros.back())) {
							config->macros.pop_back();
							return -1;
						}

						arg->idx = config->macros.size() - 1;
						break;
					default:
						assert(0);
						break;
					}
				}

				return 0;
			}
		}
	}

	err("invalid key or action");
	return -1;
}

static void parse_global_section(struct config *config, struct ini_section *section)
{
	for (size_t i = 0; i < section->entries.size(); i++) {
		struct ini_entry *ent = &section->entries[i];

		if (!strcmp(ent->key, "macro_timeout"))
			config->macro_timeout = atoi(ent->val);
		else if (!strcmp(ent->key, "macro_sequence_timeout"))
			config->macro_sequence_timeout = atoi(ent->val);
		else if (!strcmp(ent->key, "disable_modifier_guard"))
			config->disable_modifier_guard = atoi(ent->val);
		else if (!strcmp(ent->key, "oneshot_timeout"))
			config->oneshot_timeout = atoi(ent->val);
		else if (!strcmp(ent->key, "chord_hold_timeout"))
			config->chord_hold_timeout = atoi(ent->val);
		else if (!strcmp(ent->key, "chord_timeout"))
			config->chord_interkey_timeout = atoi(ent->val);
		else if (!strcmp(ent->key, "default_layout"))
			config->default_layout = ent->val;
		else if (!strcmp(ent->key, "macro_repeat_timeout"))
			config->macro_repeat_timeout = atoi(ent->val);
		else if (!strcmp(ent->key, "layer_indicator"))
			config->layer_indicator = atoi(ent->val);
		else if (!strcmp(ent->key, "overload_tap_timeout"))
			config->overload_tap_timeout = atoi(ent->val);
		else
			warn("line %zd: %s is not a valid global option", ent->lnum, ent->key);
	}
}

static void parse_id_section(struct config *config, struct ini_section *section)
{
	for (size_t i = 0; i < section->entries.size(); i++) {
		uint16_t product, vendor;

		struct ini_entry *ent = &section->entries[i];
		const char *s = ent->key;

		if (!strcmp(s, "*")) {
			config->wildcard = 1;
		} else if (strstr(s, "m:") == s) {
			assert(config->nr_ids < ARRAY_SIZE(config->ids));
			config->ids[config->nr_ids].flags = ID_MOUSE;

			snprintf(config->ids[config->nr_ids++].id, sizeof(config->ids[0].id), "%s", s+2);
		} else if (strstr(s, "k:") == s) {
			assert(config->nr_ids < ARRAY_SIZE(config->ids));
			config->ids[config->nr_ids].flags = ID_KEYBOARD;

			snprintf(config->ids[config->nr_ids++].id, sizeof(config->ids[0].id), "%s", s+2);
		} else if (strstr(s, "-") == s) {
			assert(config->nr_ids < ARRAY_SIZE(config->ids));
			config->ids[config->nr_ids].flags = ID_EXCLUDED;

			snprintf(config->ids[config->nr_ids++].id, sizeof(config->ids[0].id), "%s", s+1);
		} else if (strlen(s) < sizeof(config->ids[config->nr_ids].id)-1) {
			assert(config->nr_ids < ARRAY_SIZE(config->ids));
			config->ids[config->nr_ids].flags = ID_KEYBOARD | ID_MOUSE;

			snprintf(config->ids[config->nr_ids++].id, sizeof(config->ids[0].id), "%s", s);
		} else {
			warn("%s is not a valid device id", s);
		}
	}
}

static void parse_alias_section(struct config *config, struct ini_section *section)
{
	size_t i;

	for (i = 0; i < section->entries.size(); i++) {
		uint8_t code;
		struct ini_entry *ent = &section->entries[i];
		const char *name = ent->val;

		if ((code = lookup_keycode(ent->key))) {
			if (name && name[0]) {
				uint8_t alias_code;

				if ((alias_code = lookup_keycode(name))) {
					struct descriptor *d = &config->layers[0].keymap[code];

					d->op = OP_KEYSEQUENCE;
					d->args[0].code = alias_code;
					d->args[1].mods = 0;
				}

				config->aliases[code] = name;
			}
		} else {
			warn("failed to define alias %s, %s is not a valid keycode", name, ent->key);
		}
	}
}


static int config_parse_string(struct config *config, char *content)
{
	size_t i;
	::ini ini = ini_parse_string(content, NULL);
	if (ini.empty())
		return -1;

	/* First pass: create all layers based on section headers.  */
	for (i = 0; i < ini.size(); i++) {
		struct ini_section *section = &ini[i];

		if (section->name == "ids") {
			parse_id_section(config, section);
			section->lnum = -1;
		} else if (section->name == "aliases") {
			parse_alias_section(config, section);
			section->lnum = -1;
		} else if (section->name == "global") {
			parse_global_section(config, section);
			section->lnum = -1;
		} else {
			if (config_add_layer(config, section->name) < 0)
				warn("%s", errstr);
		}
	}

	/* Populate each layer. */
	for (i = 0; i < ini.size(); i++) {
		struct ini_section *section = &ini[i];
		std::string_view layer_name = section->name;
		layer_name = layer_name.substr(0, layer_name.find_first_of(':'));

		if (section->lnum == size_t(-1))
			continue;

		for (size_t j = 0; j < section->entries.size(); j++) {
			struct ini_entry *ent = &section->entries[j];
			if (!ent->val) {
				warn("invalid binding on line %zd", ent->lnum);
				continue;
			}

			std::string entry(layer_name);
			entry += ".";
			entry += ent->key;
			entry += " = ";
			entry += ent->val;
			if (config_add_entry(config, entry.c_str()) < 0)
				keyd_log("\tr{ERROR:} line m{%zd}: %s\n", ent->lnum, errstr);
		}
	}

	return 0;
}

static void config_init(struct config *config)
{
	size_t i;

	*config = {};
	char default_config[] =
	"[aliases]\n"

	"leftshift = shift\n"
	"rightshift = shift\n"

	"leftalt = alt\n"
	"rightalt = altgr\n"

	"leftmeta = meta\n"
	"rightmeta = meta\n"

	"leftcontrol = control\n"
	"rightcontrol = control\n"

	"[main]\n"

	"shift = layer(shift)\n"
	"alt = layer(alt)\n"
	"altgr = layer(altgr)\n"
	"meta = layer(meta)\n"
	"control = layer(control)\n"

	"[control:C]\n"
	"[shift:S]\n"
	"[meta:M]\n"
	"[alt:A]\n"
	"[altgr:G]\n";

	config_parse_string(config, default_config);

	/* In ms */
	config->chord_interkey_timeout = 50;
	config->chord_hold_timeout = 0;
	config->oneshot_timeout = 0;

	config->macro_timeout = 600;
	config->macro_repeat_timeout = 50;
}

int config_parse(struct config *config, const char *path)
{
	std::string content = read_file(path);
	if (content.empty())
		return -1;

	config_init(config);
	config->pathstr = path;
	return config_parse_string(config, content.data());
}

int config_check_match(struct config *config, const char *id, uint8_t flags)
{
	size_t i;

	for (i = 0; i < config->nr_ids; i++) {
		//Prefix match to allow matching <product>:<vendor> for backward compatibility.
		if (strstr(id, config->ids[i].id) == id) {
			if (config->ids[i].flags & ID_EXCLUDED) {
				return 0;
			} else if (config->ids[i].flags & flags) {
				return 2;
			}
		}
	}

	return config->wildcard ? 1 : 0;
}

int config_get_layer_index(const struct config *config, std::string_view name)
{
	size_t i;

	if (name.empty())
		return -1;

	for (i = 0; i < config->layers.size(); i++)
		if (config->layers[i].name == name)
			return i;

	return -1;
}

/*
 * Adds a binding of the form [<layer>.]<key> = <descriptor expression>
 * to the given config.
 */
int config_add_entry(struct config *config, const char *exp)
{
	char *keyname, *descstr, *dot, *paren, *s;
	const char *layername = "main";
	struct descriptor d;
	struct layer *layer;
	int idx;

	std::string buf = exp;
	s = buf.data();

	dot = strchr(s, '.');
	paren = strchr(s, '(');

	if (dot && dot != s && (!paren || dot < paren)) {
		layername = s;
		*dot = 0;
		s = dot+1;
	}

	parse_kvp(s, &keyname, &descstr);
	idx = config_get_layer_index(config, layername);

	if (idx == -1) {
		err("%s is not a valid layer", layername);
		return -1;
	}

	layer = &config->layers[idx];

	if (parse_descriptor(descstr, &d, config) < 0)
		return -1;

	return set_layer_entry(config, layer, keyname, &d);
}
