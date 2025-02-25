/*
 * relay-api-protocol.c - API protocol for relay to client
 *
 * Copyright (C) 2023-2024 Sébastien Helleu <flashcode@flashtux.org>
 *
 * This file is part of WeeChat, the extensible chat client.
 *
 * WeeChat is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * WeeChat is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with WeeChat.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#include <cjson/cJSON.h>

#include "../../weechat-plugin.h"
#include "../relay.h"
#include "../relay-auth.h"
#include "../relay-buffer.h"
#include "../relay-client.h"
#include "../relay-config.h"
#include "../relay-http.h"
#include "../relay-websocket.h"
#include "relay-api.h"
#include "relay-api-msg.h"
#include "relay-api-protocol.h"


/*
 * Callback for signals "buffer_*".
 */

int
relay_api_protocol_signal_buffer_cb (const void *pointer, void *data,
                                     const char *signal,
                                     const char *type_data,
                                     void *signal_data)
{
    struct t_relay_client *ptr_client;
    struct t_gui_buffer *ptr_buffer;
    struct t_gui_line *ptr_line;
    struct t_gui_line_data *ptr_line_data;
    cJSON *json;
    long lines;

    /* make C compiler happy */
    (void) data;
    (void) type_data;

    ptr_client = (struct t_relay_client *)pointer;
    if (!ptr_client || !relay_client_valid (ptr_client))
        return WEECHAT_RC_OK;

    if ((strcmp (signal, "buffer_opened") == 0)
        || (strcmp (signal, "buffer_type_changed") == 0)
        || (strcmp (signal, "buffer_moved") == 0)
        || (strcmp (signal, "buffer_merged") == 0)
        || (strcmp (signal, "buffer_unmerged") == 0)
        || (strcmp (signal, "buffer_hidden") == 0)
        || (strcmp (signal, "buffer_unhidden") == 0)
        || (strcmp (signal, "buffer_renamed") == 0)
        || (strcmp (signal, "buffer_title_changed") == 0)
        || (strncmp (signal, "buffer_localvar_", 16) == 0)
        || (strcmp (signal, "buffer_cleared") == 0)
        || (strcmp (signal, "buffer_closing") == 0))
    {
        ptr_buffer = (struct t_gui_buffer *)signal_data;
        if (!ptr_buffer)
            return WEECHAT_RC_OK;

        lines = (strcmp (signal, "buffer_opened") == 0) ? LONG_MIN : 0;
        json = relay_api_msg_buffer_to_json (
            ptr_buffer, lines, 0, RELAY_API_DATA(ptr_client, sync_colors));
        if (json)
        {
            relay_api_msg_send_event (ptr_client, signal, "buffer", "", json);
            cJSON_Delete (json);
        }
    }
    else if (strcmp (signal, "buffer_line_added") == 0)
    {
        ptr_line = (struct t_gui_line *)signal_data;
        if (!ptr_line)
            return WEECHAT_RC_OK;

        ptr_line_data = weechat_hdata_pointer (relay_hdata_line,
                                               ptr_line, "data");
        if (!ptr_line_data)
            return WEECHAT_RC_OK;

        ptr_buffer = weechat_hdata_pointer (relay_hdata_line_data,
                                            ptr_line_data, "buffer");
        if (!ptr_buffer || relay_buffer_is_relay (ptr_buffer))
            return WEECHAT_RC_OK;

        json = relay_api_msg_line_data_to_json (
            ptr_line_data, RELAY_API_DATA(ptr_client, sync_colors));
        if (json)
        {
            relay_api_msg_send_event (
                ptr_client,
                signal,
                "line",
                weechat_buffer_get_string (ptr_buffer, "full_name"),
                json);
            cJSON_Delete (json);
        }
    }

    return WEECHAT_RC_OK;
}

/*
 * Callback for hsignals "nicklist_*".
 */

int
relay_api_protocol_hsignal_nicklist_cb (const void *pointer, void *data,
                                        const char *signal,
                                        struct t_hashtable *hashtable)
{
    struct t_relay_client *ptr_client;
    struct t_gui_buffer *ptr_buffer;
    struct t_gui_nick_group *ptr_parent_group, *ptr_group;
    struct t_gui_nick *ptr_nick;
    cJSON *json;

    /* make C compiler happy */
    (void) data;

    ptr_client = (struct t_relay_client *)pointer;
    if (!ptr_client || !relay_client_valid (ptr_client))
        return WEECHAT_RC_OK;

    ptr_buffer = weechat_hashtable_get (hashtable, "buffer");
    ptr_parent_group = weechat_hashtable_get (hashtable, "parent_group");
    ptr_group = weechat_hashtable_get (hashtable, "group");
    ptr_nick = weechat_hashtable_get (hashtable, "nick");

    /* if there is no parent group (for example "root" group), ignore the signal */
    if (!ptr_parent_group)
        return WEECHAT_RC_OK;

    if ((strcmp (signal, "nicklist_group_added") == 0)
        || (strcmp (signal, "nicklist_group_changed") == 0)
        || (strcmp (signal, "nicklist_group_removing") == 0))
    {
        json = relay_api_msg_nick_group_to_json (ptr_group);
        if (json)
        {
            relay_api_msg_send_event (
                ptr_client,
                signal,
                "nick_group",
                weechat_buffer_get_string (ptr_buffer, "full_name"),
                json);
            cJSON_Delete (json);
        }
    }
    else if ((strcmp (signal, "nicklist_nick_added") == 0)
        || (strcmp (signal, "nicklist_nick_changed") == 0)
        || (strcmp (signal, "nicklist_nick_removing") == 0))
    {
        json = relay_api_msg_nick_to_json (ptr_nick);
        if (json)
        {
            relay_api_msg_send_event (
                ptr_client,
                signal,
                "nick",
                weechat_buffer_get_string (ptr_buffer, "full_name"),
                json);
            cJSON_Delete (json);
        }
    }

    return WEECHAT_RC_OK;
}

/*
 * Callback for signals "upgrade*".
 */

int
relay_api_protocol_signal_upgrade_cb (const void *pointer, void *data,
                                      const char *signal,
                                      const char *type_data,
                                      void *signal_data)
{
    struct t_relay_client *ptr_client;

    /* make C compiler happy */
    (void) data;
    (void) type_data;
    (void) signal_data;

    ptr_client = (struct t_relay_client *)pointer;
    if (!ptr_client || !relay_client_valid (ptr_client))
        return WEECHAT_RC_OK;

    if ((strcmp (signal, "upgrade") == 0)
        || (strcmp (signal, "upgrade_ended") == 0))
    {
        relay_api_msg_send_event (ptr_client, signal, NULL, NULL, NULL);
    }

    return WEECHAT_RC_OK;
}

/*
 * Callback for resource "handshake".
 *
 * Routes:
 *   POST /api/handshake
 */

RELAY_API_PROTOCOL_CALLBACK(handshake)
{
    cJSON *json_body, *json_algos, *json_algo, *json;
    const char *ptr_algo;
    char *totp_secret;
    int hash_algo_found, index_hash_algo;

    json_body = cJSON_Parse(client->http_req->body);
    if (!json_body)
        return WEECHAT_RC_ERROR;

    hash_algo_found = -1;

    json_algos = cJSON_GetObjectItem (json_body, "password_hash_algo");
    if (json_algos)
    {
        cJSON_ArrayForEach(json_algo, json_algos)
        {
            ptr_algo = (cJSON_IsString (json_algo)) ?
                cJSON_GetStringValue (json_algo) : NULL;
            if (ptr_algo)
            {
                index_hash_algo = relay_auth_password_hash_algo_search (ptr_algo);
                if ((index_hash_algo >= 0) && (index_hash_algo > hash_algo_found))
                {
                    if (weechat_string_match_list (
                            relay_auth_password_hash_algo_name[index_hash_algo],
                            (const char **)relay_config_network_password_hash_algo_list,
                            1))
                    {
                        hash_algo_found = index_hash_algo;
                    }
                }
            }
        }
    }

    json = cJSON_CreateObject ();
    if (!json)
    {
        cJSON_Delete (json_body);
        return WEECHAT_RC_ERROR;
    }

    totp_secret = weechat_string_eval_expression (
        weechat_config_string (relay_config_network_totp_secret),
        NULL, NULL, NULL);

    cJSON_AddItemToObject (
        json,
        "password_hash_algo",
        (hash_algo_found >= 0) ?
        cJSON_CreateString (relay_auth_password_hash_algo_name[hash_algo_found]) :
        cJSON_CreateNull ());
    cJSON_AddItemToObject (
        json,
        "password_hash_iterations",
        cJSON_CreateNumber (
            weechat_config_integer (relay_config_network_password_hash_iterations)));
    cJSON_AddItemToObject (json, "totp",
                           cJSON_CreateBool ((totp_secret && totp_secret[0]) ? 1 : 0));

    relay_api_msg_send_json (client, RELAY_HTTP_200_OK, json);

    free (totp_secret);
    cJSON_Delete (json);
    cJSON_Delete (json_body);

    return WEECHAT_RC_OK;
}

/*
 * Callback for resource "version".
 *
 * Routes:
 *   GET /api/version
 */

RELAY_API_PROTOCOL_CALLBACK(version)
{
    cJSON *json;
    char *version, *error;
    long number;

    json = cJSON_CreateObject ();
    if (!json)
        return WEECHAT_RC_ERROR;

    version = weechat_info_get ("version", NULL);
    cJSON_AddItemToObject (json,
                           "weechat_version", cJSON_CreateString (version));
    free (version);

    version = weechat_info_get ("version_git", NULL);
    cJSON_AddItemToObject (json,
                           "weechat_version_git", cJSON_CreateString (version));
    free (version);

    version = weechat_info_get ("version_number", NULL);
    error = NULL;
    number = strtol (version, &error, 10);
    if (error && !error[0])
    {
        cJSON_AddItemToObject (json,
                               "weechat_version_number",
                               cJSON_CreateNumber (number));
    }
    free (version);

    cJSON_AddItemToObject (json, "relay_api_version",
                           cJSON_CreateString (RELAY_API_VERSION_STR));
    cJSON_AddItemToObject (json,
                           "relay_api_version_number",
                           cJSON_CreateNumber (RELAY_API_VERSION_NUMBER));

    relay_api_msg_send_json (client, RELAY_HTTP_200_OK, json);

    cJSON_Delete (json);

    return WEECHAT_RC_OK;
}

/*
 * Callback for resource "buffers".
 *
 * Routes:
 *   GET /api/buffers
 *   GET /api/buffers/{buffer_name}
 *   GET /api/buffers/{buffer_name}/lines
 *   GET /api/buffers/{buffer_name}/lines/{line_id}
 *   GET /api/buffers/{buffer_name}/nicks
 */

RELAY_API_PROTOCOL_CALLBACK(buffers)
{
    cJSON *json;
    struct t_gui_buffer *ptr_buffer;
    long lines;
    int nicks;
    enum t_relay_api_colors colors;

    ptr_buffer = NULL;
    if (client->http_req->num_path_items > 2)
    {
        ptr_buffer = weechat_buffer_search ("==", client->http_req->path_items[2]);
        if (!ptr_buffer)
        {
            relay_api_msg_send_error_json (client, RELAY_HTTP_404_NOT_FOUND, NULL,
                                           "Buffer \"%s\" not found",
                                           client->http_req->path_items[2]);
            return WEECHAT_RC_OK;
        }
    }

    nicks = relay_http_get_param_boolean (client->http_req, "nicks", 0);
    colors = relay_api_search_colors (
        weechat_hashtable_get (client->http_req->params, "colors"));

    if (client->http_req->num_path_items > 3)
    {
        /* sub-resource of buffers */
        if (strcmp (client->http_req->path_items[3], "lines") == 0)
        {
            lines = relay_http_get_param_long (client->http_req, "lines", -100L);
            json = relay_api_msg_lines_to_json (ptr_buffer, lines, colors);
        }
        else if (strcmp (client->http_req->path_items[3], "nicks") == 0)
        {
            json = relay_api_msg_nick_group_to_json (
                weechat_hdata_pointer (relay_hdata_buffer,
                                       ptr_buffer, "nicklist_root"));
        }
        else
        {
            relay_api_msg_send_error_json (
                client, RELAY_HTTP_404_NOT_FOUND, NULL,
                "Sub-resource of buffers not found: \"%s\"",
                client->http_req->path_items[3]);
            return WEECHAT_RC_OK;
        }
    }
    else
    {
        lines = relay_http_get_param_long (client->http_req, "lines", 0L);
        if (ptr_buffer)
        {
            json = relay_api_msg_buffer_to_json (ptr_buffer, lines, nicks, colors);
        }
        else
        {
            json = cJSON_CreateArray ();
            if (!json)
                return WEECHAT_RC_ERROR;
            ptr_buffer = weechat_hdata_get_list (relay_hdata_buffer,
                                                 "gui_buffers");
            while (ptr_buffer)
            {
                cJSON_AddItemToArray (
                    json,
                    relay_api_msg_buffer_to_json (ptr_buffer,
                                                  lines, nicks, colors));
                ptr_buffer = weechat_hdata_move (relay_hdata_buffer, ptr_buffer, 1);
            }
        }
    }

    if (!json)
        goto error;

    relay_api_msg_send_json (client, RELAY_HTTP_200_OK, json);
    cJSON_Delete (json);
    return WEECHAT_RC_OK;

error:
    relay_api_msg_send_error_json (client,
                                   RELAY_HTTP_503_SERVICE_UNAVAILABLE,
                                   NULL,
                                   RELAY_HTTP_ERROR_OUT_OF_MEMORY);
    return WEECHAT_RC_OK;
}

/*
 * Callback for resource "input".
 *
 * Routes:
 *   POST /api/input/{buffer_name}
 */

RELAY_API_PROTOCOL_CALLBACK(input)
{
    cJSON *json_body, *json_buffer, *json_command;
    const char *ptr_buffer_name, *ptr_command, *ptr_commands;
    struct t_gui_buffer *ptr_buffer;
    struct t_hashtable *options;

    json_body = cJSON_Parse(client->http_req->body);
    if (!json_body)
        return WEECHAT_RC_ERROR;

    json_buffer = cJSON_GetObjectItem (json_body, "buffer");
    if (json_buffer)
    {
        if (cJSON_IsString (json_buffer))
        {
            ptr_buffer_name = cJSON_GetStringValue (json_buffer);
            ptr_buffer = weechat_buffer_search ("==", ptr_buffer_name);
            if (!ptr_buffer)
            {
                cJSON_Delete (json_body);
                relay_api_msg_send_error_json (client,
                                               RELAY_HTTP_404_NOT_FOUND, NULL,
                                               "Buffer \"%s\" not found",
                                               ptr_buffer_name);
                return WEECHAT_RC_OK;
            }
        }
    }
    else
    {
        ptr_buffer = weechat_buffer_search_main ();
    }
    if (!ptr_buffer)
    {
        cJSON_Delete (json_body);
        return WEECHAT_RC_ERROR;
    }

    json_command = cJSON_GetObjectItem (json_body, "command");
    if (!json_command || !cJSON_IsString (json_command))

    {
        cJSON_Delete (json_body);
        return WEECHAT_RC_ERROR;
    }

    ptr_command = cJSON_GetStringValue (json_command);
    if (!ptr_command)
    {
        cJSON_Delete (json_body);
        return WEECHAT_RC_ERROR;
    }

    options = weechat_hashtable_new (8,
                                     WEECHAT_HASHTABLE_STRING,
                                     WEECHAT_HASHTABLE_STRING,
                                     NULL, NULL);
    if (!options)
    {
        relay_api_msg_send_error_json (client,
                                       RELAY_HTTP_503_SERVICE_UNAVAILABLE,
                                       NULL,
                                       RELAY_HTTP_ERROR_OUT_OF_MEMORY);
        cJSON_Delete (json_body);
        return WEECHAT_RC_OK;
    }

    ptr_commands = weechat_config_string (relay_config_network_commands);
    if (ptr_commands && ptr_commands[0])
        weechat_hashtable_set (options, "commands", ptr_commands);

    /*
     * delay the execution of command after we go back in the WeeChat
     * main loop (some commands like /upgrade executed now can cause
     * a crash)
     */
    weechat_hashtable_set (options, "delay", "1");

    /* execute the command, with the delay */
    weechat_command_options (ptr_buffer, ptr_command, options);

    weechat_hashtable_free (options);
    cJSON_Delete (json_body);

    relay_api_msg_send_json (client, RELAY_HTTP_204_NO_CONTENT, NULL);

    return WEECHAT_RC_OK;
}

/*
 * Callback for resource "ping".
 *
 * Routes:
 *   POST /api/ping
 */

RELAY_API_PROTOCOL_CALLBACK(ping)
{
    cJSON *json, *json_body, *json_data;
    const char *ptr_data;

    ptr_data = NULL;
    json_body = cJSON_Parse(client->http_req->body);
    if (json_body)
    {
        json_data = cJSON_GetObjectItem (json_body, "data");
        if (json_data && cJSON_IsString (json_data))
            ptr_data = cJSON_GetStringValue (json_data);
    }

    if (ptr_data)
    {
        json = cJSON_CreateObject ();
        if (!json)
        {
            cJSON_Delete (json_body);
            return WEECHAT_RC_ERROR;
        }
        cJSON_AddItemToObject (json, "data",
                               cJSON_CreateString ((ptr_data) ? ptr_data : ""));
        relay_api_msg_send_json (client, RELAY_HTTP_200_OK, json);
        cJSON_Delete (json);
        cJSON_Delete (json_body);
    }
    else
    {
        relay_api_msg_send_json (client, RELAY_HTTP_204_NO_CONTENT, NULL);
    }

    return WEECHAT_RC_OK;
}

/*
 * Callback for resource "sync".
 *
 * Routes:
 *   POST /api/sync
 */

RELAY_API_PROTOCOL_CALLBACK(sync)
{
    cJSON *json_body, *json_sync, *json_nicks, *json_colors;

    if (client->websocket != RELAY_CLIENT_WEBSOCKET_READY)
    {
        relay_api_msg_send_error_json (
            client,
            RELAY_HTTP_403_FORBIDDEN,
            NULL,
            "Sync resource is available only with a websocket connection");
        return WEECHAT_RC_OK;
    }

    RELAY_API_DATA(client, sync_enabled) = 1;
    RELAY_API_DATA(client, sync_nicks) = 1;
    RELAY_API_DATA(client, sync_colors) = RELAY_API_COLORS_ANSI;

    json_body = cJSON_Parse(client->http_req->body);
    if (json_body)
    {
        json_sync = cJSON_GetObjectItem (json_body, "sync");
        if (json_sync && cJSON_IsBool (json_sync))
            RELAY_API_DATA(client, sync_enabled) = (cJSON_IsTrue (json_sync)) ? 1 : 0;
        json_nicks = cJSON_GetObjectItem (json_body, "nicks");
        if (json_nicks && cJSON_IsBool (json_nicks))
            RELAY_API_DATA(client, sync_nicks) = (cJSON_IsTrue (json_nicks)) ? 1 : 0;
        json_colors = cJSON_GetObjectItem (json_body, "colors");
        if (json_colors && cJSON_IsString (json_colors))
            RELAY_API_DATA(client, sync_colors) = relay_api_search_colors (
                cJSON_GetStringValue (json_colors));
    }

    if (RELAY_API_DATA(client, sync_enabled))
        relay_api_hook_signals (client);
    else
        relay_api_unhook_signals (client);

    relay_api_msg_send_json (client, RELAY_HTTP_204_NO_CONTENT, NULL);

    return WEECHAT_RC_OK;
}

/*
 * Reads JSON string from a client: when connected via websocket (persistent
 * connection), the client is sending JSON data as a request, which is
 * converted to HTTP request by this function, before calling the function
 * relay_api_protocol_recv_http.
 *
 * Example of JSON received:
 *
 * {
 *     "request": "POST /api/input",
 *     "body": {
 *         "buffer": "irc.libera.#weechat",
 *         "command": "hello!"
 *     }
 * }
 *
 * It is converted to an HTTP request which could have been:
 *
 * POST /api/input HTTP/1.1
 * Content-Length: 53
 * Content-Type: application/json
 *
 * {"buffer": "irc.libera.#weechat","command": "hello!"}
 */

void
relay_api_protocol_recv_json (struct t_relay_client *client, const char *json)
{
    cJSON *json_obj, *json_request, *json_body;
    char *string_body;
    int length;

    relay_http_request_reinit (client->http_req);

    json_obj = cJSON_Parse(json);
    if (!json_obj)
        goto error;

    json_request = cJSON_GetObjectItem (json_obj, "request");
    if (!json_request)
        goto error;

    if (!relay_http_parse_method_path (client->http_req,
                                       cJSON_GetStringValue (json_request)))
    {
        goto error;
    }

    json_body = cJSON_GetObjectItem (json_obj, "body");
    if (json_body)
    {
        string_body = cJSON_PrintUnformatted (json_body);
        if (string_body)
        {
            length = strlen (string_body);
            client->http_req->body = malloc (length + 1);
            if (client->http_req->body)
            {
                memcpy (client->http_req->body, string_body, length + 1);
                client->http_req->content_length = length;
                client->http_req->body_size = length;
            }
            free (string_body);
        }
    }

    relay_api_protocol_recv_http (client);
    goto end;

error:
    relay_api_msg_send_json (client, RELAY_HTTP_400_BAD_REQUEST, NULL);

end:
    if (json_obj)
        cJSON_Delete (json_obj);
}

/*
 * Reads a HTTP request from a client.
 */

void
relay_api_protocol_recv_http (struct t_relay_client *client)
{
    int i, return_code, num_args;
    struct t_relay_api_protocol_cb protocol_cb[] = {
        /* method, resource, auth, min args, max args, callback */
        { "POST", "handshake", 0, 0, 0, &relay_api_protocol_cb_handshake },
        { "GET",  "version",   1, 0, 0, &relay_api_protocol_cb_version   },
        { "GET",  "buffers",   1, 0, 3, &relay_api_protocol_cb_buffers   },
        { "POST", "input",     1, 0, 0, &relay_api_protocol_cb_input     },
        { "POST", "ping",      1, 0, 0, &relay_api_protocol_cb_ping      },
        { "POST", "sync",      1, 0, 0, &relay_api_protocol_cb_sync      },
        { NULL,   NULL,        0, 0, 0, NULL                             },
    };

    if (!client->http_req || RELAY_CLIENT_HAS_ENDED(client))
        return;

    /* display debug message */
    if (weechat_relay_plugin->debug >= 2)
    {
        weechat_printf (NULL,
                        "%s: recv from client %s%s%s: \"%s %s\", body: \"%s\"",
                        RELAY_PLUGIN_NAME,
                        RELAY_COLOR_CHAT_CLIENT,
                        client->desc,
                        RELAY_COLOR_CHAT,
                        client->http_req->method,
                        client->http_req->path,
                        client->http_req->body);
    }

    if ((client->http_req->num_path_items < 2) || !client->http_req->path_items
        || !client->http_req->path_items[0] || !client->http_req->path_items[1])
    {
        goto error404;
    }

    if (strcmp (client->http_req->path_items[0], "api") != 0)
        goto error404;

    num_args = client->http_req->num_path_items - 2;

    for (i = 0; protocol_cb[i].resource; i++)
    {
        if ((strcmp (protocol_cb[i].method, client->http_req->method) != 0)
            || (strcmp (protocol_cb[i].resource, client->http_req->path_items[1]) != 0))
            continue;

        if (protocol_cb[i].auth_required
            && (client->status != RELAY_STATUS_CONNECTED)
            && !relay_http_check_auth (client))
        {
            relay_client_set_status (client, RELAY_STATUS_AUTH_FAILED);
            return;
        }

        if (num_args < protocol_cb[i].min_args)
        {
            if (weechat_relay_plugin->debug >= 1)
            {
                weechat_printf (
                    NULL,
                    _("%s%s: too few arguments received from client "
                      "%s%s%s for resource \"%s\" "
                      "(received: %d arguments, expected: at least %d)"),
                    weechat_prefix ("error"),
                    RELAY_PLUGIN_NAME,
                    RELAY_COLOR_CHAT_CLIENT,
                    client->desc,
                    RELAY_COLOR_CHAT,
                    client->http_req->path_items[1],
                    num_args,
                    protocol_cb[i].min_args);
            }
            goto error404;
        }

        if (num_args > protocol_cb[i].max_args)
        {
            if (weechat_relay_plugin->debug >= 1)
            {
                weechat_printf (
                    NULL,
                    _("%s%s: too many arguments received from client "
                      "%s%s%s for resource \"%s\" "
                      "(received: %d arguments, expected: at most %d)"),
                    weechat_prefix ("error"),
                    RELAY_PLUGIN_NAME,
                    RELAY_COLOR_CHAT_CLIENT,
                    client->desc,
                    RELAY_COLOR_CHAT,
                    client->http_req->path_items[1],
                    num_args,
                    protocol_cb[i].max_args);
            }
            goto error404;
        }

        return_code = (int) (protocol_cb[i].cmd_function) (client);
        if (return_code == WEECHAT_RC_OK)
            return;
        else
            goto error400;
    }

    goto error404;

error400:
    relay_api_msg_send_json (client, RELAY_HTTP_400_BAD_REQUEST, NULL);
    goto error;

error404:
    relay_api_msg_send_json (client, RELAY_HTTP_404_NOT_FOUND, NULL);
    goto error;

error:
    if (weechat_relay_plugin->debug >= 1)
    {
        weechat_printf (NULL,
                        _("%s%s: failed to execute route \"%s %s\" "
                          "for client %s%s%s"),
                        weechat_prefix ("error"),
                        RELAY_PLUGIN_NAME,
                        client->http_req->method,
                        client->http_req->path,
                        RELAY_COLOR_CHAT_CLIENT,
                        client->desc,
                        RELAY_COLOR_CHAT);
    }
}
