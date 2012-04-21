
#include "config.h"

#include <gst-stream-server/gss-server.h>
#include <gst-stream-server/gss-config.h>
#include <gst-stream-server/gss-form.h>
#include <gst-stream-server/gss-html.h>

#include <glib/gstdio.h>
#include <glib-object.h>


#define BASE "/"


static void admin_callback (SoupServer *server, SoupMessage *msg,
    const char *path, GHashTable *query, SoupClientContext *client,
    gpointer user_data);

Field control_fields[] = {
  { FIELD_SECTION, NULL, "Control" },
  { FIELD_CHECKBOX, "enable_streaming", "Enable Public Streaming", "on", 1 },
  { FIELD_SECTION, NULL, "Stream #0" },
  { FIELD_TEXT_INPUT, "stream0_name", "Stream Name", "stream0", 0 },
  { FIELD_SELECT, "stream0_type", "Connection type", "ew-follow", 0,
    {
      { "ew-follow", "E1000/S1000 follower" },
      { "http-follow", "HTTP stream follower" },
      { "ew-contrib", "Entropy Wave contributor" },
      { "icecast", "Icecast contributor" }
    }
  },
  { FIELD_TEXT_INPUT, "stream0_url", "Stream URL or E1000 IP address", "10.0.2.40", 0 },
  //{ FIELD_TEXT_INPUT, "stream0_width", "Width", "640", 0 },
  //{ FIELD_TEXT_INPUT, "stream0_height", "Height", "360", 0 },
  //{ FIELD_TEXT_INPUT, "stream0_bitrate", "Bitrate", "700000", 0 },
  { FIELD_SECTION, NULL, "Stream #1" },
  { FIELD_TEXT_INPUT, "stream1_name", "Stream Name", "stream1", 0 },
  { FIELD_SELECT, "stream1_type", "Connection type", "ew-follow", 0,
    {
      { "ew-follow", "E1000/S1000 follower" },
      { "http-follow", "HTTP stream follower" },
      { "ew-contrib", "Entropy Wave contributor" },
      { "icecast", "Icecast contributor" }
    }
  },
  { FIELD_TEXT_INPUT, "stream1_url", "Stream URL or E1000 IP address", "", 0 },
  { FIELD_SECTION, NULL, "Stream #2" },
  { FIELD_TEXT_INPUT, "stream2_name", "Stream Name", "stream2", 0 },
  { FIELD_SELECT, "stream2_type", "Connection type", "ew-follow", 0,
    {
      { "ew-follow", "E1000/S1000 follower" },
      { "http-follow", "HTTP stream follower" },
      { "ew-contrib", "Entropy Wave contributor" },
      { "icecast", "Icecast contributor" }
    }
  },
  { FIELD_TEXT_INPUT, "stream2_url", "Stream URL or E1000 IP address", "", 0 },
  { FIELD_SECTION, NULL, "Stream #3" },
  { FIELD_TEXT_INPUT, "stream3_name", "Stream Name", "stream3", 0 },
  { FIELD_SELECT, "stream3_type", "Connection type", "ew-follow", 0,
    {
      { "ew-follow", "E1000/S1000 follower" },
      { "http-follow", "HTTP stream follower" },
      { "ew-contrib", "Entropy Wave contributor" },
      { "icecast", "Icecast contributor" }
    }
  },
  { FIELD_TEXT_INPUT, "stream3_url", "Stream URL or E1000 IP address", "", 0 },
  { FIELD_SECTION, NULL, "Stream #4" },
  { FIELD_TEXT_INPUT, "stream4_name", "Stream Name", "stream4", 0 },
  { FIELD_SELECT, "stream4_type", "Connection type", "ew-follow", 0,
    {
      { "ew-follow", "E1000/S1000 follower" },
      { "http-follow", "HTTP stream follower" },
      { "ew-contrib", "Entropy Wave contributor" },
      { "icecast", "Icecast contributor" }
    }
  },
  { FIELD_TEXT_INPUT, "stream4_url", "Stream URL or E1000 IP address", "", 0 },
  { FIELD_SUBMIT, "submit", "Update Configuration", NULL, 0 },
  { FIELD_NONE }
};

Field server_fields[] = {
  { FIELD_SECTION, NULL, "HTTP Server Configuration" },
  { FIELD_TEXT_INPUT, "server_name", "Server Hostname", "127.0.0.1", 0 },
  { FIELD_TEXT_INPUT, "server_port", "Server Port", "80", 0 },
  { FIELD_TEXT_INPUT, "max_connections", "Max Connections", "10000", 0 },
  { FIELD_TEXT_INPUT, "max_bandwidth", "Max Bandwidth (kbytes/sec)", "100000",
    0 },
  { FIELD_SUBMIT, "submit", "Update Configuration", NULL, 1 },
  { FIELD_NONE }
};

Field network_fields[] = {
  { FIELD_CHECKBOX, "enable_os_network", "Configure network in OS", "off", 0 },
  { FIELD_SECTION, NULL, "Primary Ethernet" },
  { FIELD_TEXT_INPUT, "eth0_name", "eth0 Name", "e1000-0", 0 },
  { FIELD_RADIO, "eth0_config", "IP Address", "dhcp", 0,
    {
      { "dhcp", "Automatic (DHCP)" },
      { "manual", "Manual" },
    }
  },
  { FIELD_TEXT_INPUT, "eth0_ipaddr", "Address", "10.0.2.50", 1 },
  { FIELD_TEXT_INPUT, "eth0_netmask", "Netmask", "255.255.255.0", 1 },
  { FIELD_TEXT_INPUT, "eth0_gateway", "Gateway", "10.0.2.1", 1 },
  { FIELD_SECTION, NULL, "Secondary Ethernet" },
  { FIELD_TEXT_INPUT, "eth1_name", "eth1 Name", "e1000-1", 0 },
  { FIELD_RADIO, "eth1_config", "IP Address", "dhcp", 0,
    {
      { "dhcp", "Automatic (DHCP)" },
      { "manual", "Manual" },
    }
  },
  { FIELD_TEXT_INPUT, "eth1_ipaddr", "Address", "10.0.2.50", 1 },
  { FIELD_TEXT_INPUT, "eth1_netmask", "Netmask", "255.255.255.0", 1 },
  { FIELD_TEXT_INPUT, "eth1_gateway", "Gateway", "10.0.2.1", 1 },
  { FIELD_SECTION, NULL, "DNS server" },
  { FIELD_TEXT_INPUT, "dns1", "DNS #1", "", 1 },
  { FIELD_TEXT_INPUT, "dns2", "DNS #2", "", 1 },
  { FIELD_SECTION, NULL, "NTP server" },
  { FIELD_TEXT_INPUT, "ntp", "NTP #1", "", 1 },
  { FIELD_HIDDEN, "reboot", NULL, "yes", 0 },
  { FIELD_SUBMIT, "submit", "Update Configuration and Reboot", NULL, 0 },
  { FIELD_NONE }
};

Field access_fields[] = {
  { FIELD_SECTION, NULL, "Access Restrictions" },
  { FIELD_TEXT_INPUT, "hosts_allow", "Allowed Hosts (admin)", "0.0.0.0/0", 0 },
  //{ FIELD_TEXT_INPUT, "hosts_allow_stream", "Streaming", "0.0.0.0/0", 0 },
  { FIELD_SUBMIT, "submit", "Update", NULL, 1 },
  { FIELD_NONE }
};

Field admin_password_fields[] = {
  { FIELD_SECTION, NULL, "Administrator Password" },
  { FIELD_PASSWORD, "admin_token0", "Password", "", 0 },
  { FIELD_PASSWORD, "admin_token1", "Retype", "", 0 },
  { FIELD_SUBMIT, "submit", "Change Password", NULL, 1 },
  { FIELD_NONE }
};

Field editor_password_fields[] = {
  { FIELD_SECTION, NULL, "Editor Password" },
  { FIELD_PASSWORD, "editor_token0", "Password", "", 0 },
  { FIELD_PASSWORD, "editor_token1", "Retype", "", 0 },
  { FIELD_SUBMIT, "submit", "Change Password", NULL, 1 },
  { FIELD_NONE }
};

Field configuration_file_fields[] = {
  { FIELD_SECTION, NULL, "Configuration File" },
  { FIELD_FILE, "config_file", "Upload Config", "config", 0 },
  { FIELD_SUBMIT, "submit", "Upload Configuration", NULL, 1 },
  { FIELD_NONE }
};

Field firmware_file_fields[] = {
  { FIELD_SECTION, NULL, "Firmware Update" },
  { FIELD_FILE, "firmware_file", "Upload File", "config", 0 },
  { FIELD_SUBMIT, "submit", "Update Firmware", NULL, 1 },
  { FIELD_NONE }
};

Field certificate_file_fields[] = {
  { FIELD_SECTION, NULL, "Certificate Upload" },
  { FIELD_FILE, "cert_file", "Upload Certificate", "server.crt", 0 },
  { FIELD_FILE, "key_file", "Upload Key", "server.key", 0 },
  { FIELD_SUBMIT, "submit", "Update Files", NULL, 1 },
  { FIELD_NONE }
};

Field reboot_fields[] = {
  { FIELD_SECTION, NULL, "Reboot Machine" },
  { FIELD_HIDDEN, "reboot", NULL, "yes", 0 },
  { FIELD_SUBMIT, "submit", "Reboot", NULL, 1 },
  { FIELD_NONE }
};

Field poweroff_fields[] = {
  { FIELD_SECTION, NULL, "Power Off Machine" },
  { FIELD_HIDDEN, "poweroff", NULL, "yes", 0 },
  { FIELD_SUBMIT, "submit", "Power Off", NULL, 1 },
  { FIELD_NONE }
};



static void
admin_header (EwServer *server, GString *s, const char *session_id)
{

  ew_html_header (s, "S1000 Configuration");

  g_string_append_printf(s,
      "<div id=\"header\"><img src=\"" BASE "images/template_header_nologo.png\" width=\"812\" height=\"36\" border=\"0\" alt=\"\" />\n");

  g_string_append_printf(s,
      "<img src=\"" BASE "images/template_s1000.png\" width=\"812\" height=\"58\" border=\"0\" alt=\"\"/>\n");

  g_string_append_printf(s,
      "</div><!-- end header div -->\n"
      "<div id=\"nav\">\n"
      "<div><a href=\"/admin?session_id=%s\"><img src=\"" BASE "images/button_main.png\" width=\"142\" height=\"32\" border=\"0\" alt=\"MAIN\" /></a></div>\n",
      session_id);

  g_string_append_printf(s,
      "<div><a href=\"/admin/network?session_id=%s\"><img src=\"" BASE "images/button_network.png\" width=\"142\" height=\"32\" border=\"0\" alt=\"NETWORK\" /></a></div>\n"
      "<div><a href=\"/admin/access?session_id=%s\"><img src=\"" BASE "images/button_access.png\" width=\"142\" height=\"32\" border=\"0\" alt=\"ACCESS\" /></a></div>\n"
      "<div><a href=\"/admin/admin?session_id=%s\"><img src=\"" BASE "images/button_admin.png\" width=\"142\" height=\"32\" border=\"0\" alt=\"ADMIN\" /></a></div>\n"
      "<div><a href=\"/admin/log?session_id=%s\"><img src=\"" BASE "images/button_log.png\" width=\"142\" height=\"32\" border=\"0\" alt=\"LOG\" /></a></div>\n"
      "</div><!-- end nav div -->\n",
      session_id, session_id, session_id, session_id);

  g_string_append_printf(s, "<div id=\"content\">\n");
}


enum {
  ADMIN_NONE = 0,
  ADMIN_CONTROL,
  ADMIN_SERVER,
  ADMIN_NETWORK,
  ADMIN_ADMIN,
  ADMIN_LOG,
  ADMIN_STATUS,
  ADMIN_CONFIG,
  ADMIN_ACCESS
};

typedef struct _AdminPage AdminPage;
struct _AdminPage {
  const char *location;
  int type;
  gboolean admin_only;
};

AdminPage admin_pages[] = {
  { "/admin", ADMIN_CONTROL, FALSE },
  { "/admin/", ADMIN_CONTROL, FALSE },
  { "/admin/server", ADMIN_SERVER, TRUE },
  { "/admin/admin", ADMIN_ADMIN, TRUE },
  { "/admin/reboot", ADMIN_ADMIN, TRUE },
  { "/admin/poweroff", ADMIN_ADMIN, TRUE },
  { "/admin/admin_password", ADMIN_ADMIN, TRUE },
  { "/admin/editor_password", ADMIN_ADMIN, FALSE },
  { "/admin/network", ADMIN_NETWORK, TRUE },
  { "/admin/log", ADMIN_LOG, TRUE },
  { "/admin/status", ADMIN_STATUS, FALSE },
  { "/admin/config", ADMIN_CONFIG, TRUE },
  { "/admin/access", ADMIN_ACCESS, TRUE }
};


static void
admin_callback (SoupServer *server, SoupMessage *msg,
    const char *path, GHashTable *query, SoupClientContext *client,
    gpointer user_data)
{
  const char *mime_type = "text/html";
  char *content;
  GString *s;
  EwServer *ewserver = (EwServer *)user_data;
  int type;
  int i;
  EwSession *session;

#if 0
  if (msg->method == SOUP_METHOD_GET) {
    g_print("GET %s\n", path);
  } else if (msg->method == SOUP_METHOD_POST) {
    g_print("POST %s\n", path);
  }
#endif

#if 0
  if (query) {
    GHashTableIter iter;
    char *key, *value;
    g_hash_table_iter_init (&iter, query);
    while (g_hash_table_iter_next (&iter, (gpointer)&key, (gpointer)&value)) {
      g_print("%s=%s\n", key, value);
    }
  }
#endif

  if (!ew_addr_address_check (client)) {
    ew_html_error_404 (msg);
    return;
  }

  if (server == ewserver->server) {
    if (ew_addr_is_localhost (client)) {
      session = ew_session_message_get_session (msg, query);
      if (session == NULL) {
        EwSession *session;
        char *location;

        session = ew_session_new ("admin");

        location = g_strdup_printf("/admin?session_id=%s", session->session_id);

        soup_message_headers_append (msg->response_headers,
            "Location", location);
        soup_message_set_response (msg, "text/plain", SOUP_MEMORY_STATIC, "", 0);
        soup_message_set_status (msg, SOUP_STATUS_SEE_OTHER);

        g_free (location);

        return;
      }
    } else {
      session = NULL;
    }
  } else {
    session = ew_session_message_get_session (msg, query);
  }

  if (session == NULL) {
    char *s;
    char *location;

    s = g_uri_escape_string (path, NULL, FALSE);
    if (server == ewserver->server) {
      char *host;
      char *colon;
      
      host = strdup (soup_message_headers_get (msg->request_headers, "Host"));
      colon = strchr (host, ':');
      if (colon) colon[0] = 0;

      location = g_strdup_printf("https://%s:%d/login?redirect_url=%s",
          host, soup_server_get_port(ewserver->ssl_server), s);

      g_free (host);
    } else {
      location = g_strdup_printf("/login?redirect_url=%s", s);
    }

    soup_message_headers_append (msg->response_headers,
        "Location", location);
    soup_message_set_status (msg, SOUP_STATUS_TEMPORARY_REDIRECT);

    g_free (s);
    g_free (location);

    return;
  }

  type = ADMIN_NONE;
  for(i=0;i<G_N_ELEMENTS(admin_pages);i++){
    if (g_str_equal (admin_pages[i].location, path)) {
      type = admin_pages[i].type;
      break;
    }
  }
  if (type == ADMIN_NONE) {
    ew_html_error_404 (msg);
    return;
  }

  if (msg->method != SOUP_METHOD_GET && msg->method != SOUP_METHOD_POST) {
    soup_message_set_status (msg, SOUP_STATUS_NOT_IMPLEMENTED);
    return;
  }

  ew_session_touch (session);

  if (msg->method == SOUP_METHOD_POST) {
    //g_print("POST to %s\n", path);
    ew_config_handle_post (ewserver->config, msg);
  }

  s = g_string_new ("");

  if (type == ADMIN_STATUS) {
    mime_type = "text/plain";
    g_string_append (s, "OK\n");
  } else if (type == ADMIN_CONFIG) {
    mime_type = "text/plain";
    ew_config_hash_to_string (s, ewserver->config->hash);
  } else {
    admin_header (ewserver, s, session->session_id);

    if (msg->method == SOUP_METHOD_POST) {
      g_string_append_printf(s, "<br />Configuration Updated!<br /><br />\n");
    }

    switch (type) {
      case ADMIN_CONTROL:
        ew_config_form_add_form (ewserver, s, "/admin", "Control", control_fields, session);
        break;
      case ADMIN_NETWORK:
        ew_config_form_add_form (ewserver, s, "/admin/network", "Network Configuration", network_fields, session);
        break;
      case ADMIN_SERVER:
        ew_config_form_add_form (ewserver, s, "/admin/server", "HTTP Server Configuration", server_fields, session);
        break;
      case ADMIN_LOG:
        {
          GList *g;
          g_string_append_printf (s, "<pre>\n");
          for(g=ewserver->messages;g;g=g_list_next(g)) {
            g_string_append_printf (s, "%s\n", (char *)g->data);
          }
          g_string_append_printf (s, "</pre>\n");
        }
        break;
      case ADMIN_ADMIN:
        g_string_append_printf (s, "Firmware Version: %s<br />\n",
            ew_config_get (ewserver->config, "version"));
        g_string_append_printf (s, "Configuration File: <a href=\"/admin/config?session_id=%s\">LINK</a><br />\n",
            session->session_id);
        ew_config_form_add_form (ewserver, s, "/admin/admin_password", "Admin Password",
            admin_password_fields, session);
        ew_config_form_add_form (ewserver, s, "/admin/editor_password", "Editor Password",
            editor_password_fields, session);
        ew_config_form_add_form (ewserver, s, "/admin/upload_config", "Configuration File",
            configuration_file_fields, session);
        ew_config_form_add_form (ewserver, s, "/admin/status", "Firmware Update",
            firmware_file_fields, session);
        ew_config_form_add_form (ewserver, s, "/admin/status", "Upload Certificate",
            certificate_file_fields, session);
        ew_config_form_add_form (ewserver, s, "/admin/reboot", "Reboot", reboot_fields, session);
        ew_config_form_add_form (ewserver, s, "/admin/poweroff", "Power Off", poweroff_fields, session);
        break;
      case ADMIN_ACCESS:
        ew_config_form_add_form (ewserver, s, "/admin/access", "Access Restrictions",
            access_fields, session);
        break;
      default:
        break;
    }

    g_string_append (s, "</div><!-- end content div -->\n");
    ew_html_footer (s, session->session_id);
  }

  content = g_string_free (s, FALSE);

  soup_message_headers_replace (msg->response_headers, "Keep-Alive",
      "timeout=5, max=100");

  if (msg->method == SOUP_METHOD_POST) {
#if 0
    soup_message_headers_append (msg->response_headers,
        "Location", "/admin/access?login_token=0xdeadbeef");
    
    //soup_message_set_status (msg, SOUP_STATUS_SEE_OTHER);
#endif
    soup_message_set_status (msg, SOUP_STATUS_OK);
  } else {
    soup_message_set_status (msg, SOUP_STATUS_OK);
  }

  soup_message_set_response (msg, mime_type, SOUP_MEMORY_TAKE,
      content, strlen(content));
}


void
ew_server_add_admin_callbacks (EwServer *server, SoupServer *soupserver)
{
  soup_server_add_handler (soupserver, "/admin", admin_callback,
      server, NULL);
}


EwConfigDefault config_defaults[] = {
  { "mode", "streamer" },

  /* master enable */
  { "enable_streaming", "on" },

  /* web server config */
  { "server_name", "" },
  { "max_connections", "10000" },
  { "max_bandwidth", "100000" },

  /* Ethernet config */
  { "eth0_name", "entropywave" },
  { "eth0_config", "manual" },
  { "eth0_ipaddr", "192.168.0.10" },
  { "eth0_netmask", "255.255.255.0" },
  { "eth0_gateway", "192.168.0.1" },
  { "eth1_name", "entropywave" },
  { "eth1_config", "manual" },
  { "eth1_ipaddr", "192.168.1.10" },
  { "eth1_netmask", "255.255.255.0" },
  { "eth1_gateway", "192.168.1.1" },

  { NULL, NULL }
};


