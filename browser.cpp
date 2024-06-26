#include "browser.h"

int TITLE_LENGTH = 20;
int LIMIT_TAB = 8;
ustring app_name = "CamBrowser";

ustring remove_http_string(ustring address) {
	string r = address;
	if (address.find("http://www.") == 0) {
		r.erase(0, 11);
	}	
	else if (address.find("https://www.") == 0) {
		r.erase(0, 12);
	}
	else if (address.find("http://") == 0) {
		r.erase(0, 7);
	}
	else if (address.find("https://") == 0) {
		r.erase(0, 8);
	}
	if (r.back() == '/') {
		r.pop_back();
	}
		
	return r;
}

void find_and_replace(ustring& content, const string& search, const string& replace) {
	auto pos = content.find(search);
	while(pos != std::string::npos) {
		content.replace(pos, search.length(), replace);
		pos = content.find(search, pos);
	}
}

Browser::~Browser() {
	
}

static gboolean webview_decide_policy(WebKitWebView *webview, WebKitPolicyDecision *decision, 
	WebKitPolicyDecisionType type,  void* thisclass) {
	switch (type) {
	case WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION: {
		auto navigation_decision = WEBKIT_NAVIGATION_POLICY_DECISION(decision);
		auto navigation_action = webkit_navigation_policy_decision_get_navigation_action(navigation_decision);
		int modifiers = webkit_navigation_action_get_modifiers(navigation_action);
		
		auto request = webkit_navigation_action_get_request(navigation_action);
		auto address = webkit_uri_request_get_uri(request);
		
		if (address) {
			Browser* browser = (Browser*)thisclass;
			
			if (modifiers == GDK_CONTROL_MASK) {
				webkit_policy_decision_ignore(decision);
				browser->create_new_tab(address, false);
			}
		}
				
		break;
	}
	case WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION:
		break;
	
	case WEBKIT_POLICY_DECISION_TYPE_RESPONSE: {
	
		break;
	}
	
	default:
		return false;
	}
	
	return true;
}

bool Browser::checkBlackListAddress(std::string address) {
	return blacklist.isInBlackList(address);
}


static GtkWidget* webview_create(WebKitWebView* webview, WebKitNavigationAction *navigation_action, void* thisclass) {
	auto request = webkit_navigation_action_get_request(navigation_action);
	const gchar* address = webkit_uri_request_get_uri(request);
	if (address) {
		Browser* browser = (Browser*)thisclass;
		if (browser->checkBlackListAddress(address)) {
			return NULL;
		}
		browser->create_new_tab(address, false); // false: dont switch to last tab
	}
	
	return NULL;
}

static void webview_load_change (WebKitWebView* webview, WebKitLoadEvent load_event, void* thisclass) {
			
	switch (load_event) {
		case WEBKIT_LOAD_STARTED: {
			const gchar* address = webkit_web_view_get_uri(webview);
			if (address) {
				Browser* browser = (Browser*)thisclass;
				
				if (browser->checkBlackListAddress(address)) {
					return;
				}
				
				auto current_tab_webview = browser->get_webview_from_current_tab();
				if (webview == current_tab_webview) {
					browser->set_title(app_name + " - " + address);
					browser->set_address(address);
					browser->check_address_bookmark(address);
				}
				
				ustring tab_label_text = "↻ " + remove_http_string(address);
				browser->set_tab_label(webview, tab_label_text);
			}
			
			break;
		}
			
		case WEBKIT_LOAD_REDIRECTED:
			break;
			
		case WEBKIT_LOAD_COMMITTED: {
			break;
		}
			
		case WEBKIT_LOAD_FINISHED: {
			const gchar* title = webkit_web_view_get_title(webview);
			const gchar* address = webkit_web_view_get_uri(webview);
			
			if (title && *title && address) {
				Browser* browser = (Browser*)thisclass;
				
				browser->set_tab_label(webview, title);
				browser->save_history(title, address);
			}
			
			break;
		} 
	}
}

static void webview_load_fail(WebKitWebView* webview, WebKitLoadEvent load_event, void* thisclass) {
	switch (load_event) {
		case WEBKIT_LOAD_STARTED: {
			break;
		}
			
		case WEBKIT_LOAD_REDIRECTED:
			break;
			
		case WEBKIT_LOAD_COMMITTED: {
			break;
		}
			
		case WEBKIT_LOAD_FINISHED: {
			break;
		} 
	}
}

static gboolean webview_enter_fullscreen (WebKitWebView* web_view, void* thisclass) {
	Browser* browser = (Browser*)thisclass;
	browser->make_fullscreen();
	return false;
}

static gboolean webview_leave_fullscreen (WebKitWebView* web_view, void* thisclass) {
	Browser* browser = (Browser*)thisclass;
	browser->make_unfullscreen();
	return false;
}

void Browser::make_fullscreen() {
	fullscreen();
	
	menu_box.hide();
	notebook.set_show_tabs(false);
	browser_box_side_bar.hide();
	
	showing_side_bar = false;
	is_fullscreen = true;
}

void Browser::make_unfullscreen() {
	unfullscreen();
	
	menu_box.show();
	notebook.set_show_tabs(true);
	
	is_fullscreen = false;
}

WebKitWebView* Browser::get_webview_from_current_tab() {
	int tab_index = notebook.get_current_page();
	Widget* widget = notebook.get_nth_page(tab_index);
	WebKitWebView* webview = (WebKitWebView*)Glib::unwrap(widget);
	return webview;
}


WebKitWebContext* Browser::get_web_context(bool is_private) {
	
	if (is_private) {
		return web_context;
	}
	else {
		if (switch_context == 0) {
			switch_context++;
			return web_context;
		}
		else {
			switch_context = 0;
			return web_context; 
		}
	}
}

WebKitWebView* Browser::create_new_webview() {
	auto web_context = get_web_context();
	WebKitWebView* webview = WEBKIT_WEB_VIEW(webkit_web_view_new_with_context(web_context));
	
	g_signal_connect(webview, "load-changed", G_CALLBACK(webview_load_change), this);
	g_signal_connect(webview, "load-failed", G_CALLBACK(webview_load_fail), this);
	g_signal_connect(webview, "create", G_CALLBACK(webview_create), this);
	g_signal_connect(webview, "decide-policy", G_CALLBACK(webview_decide_policy), this);
	g_signal_connect(webview, "enter-fullscreen", G_CALLBACK(webview_enter_fullscreen), this);
	g_signal_connect(webview, "leave-fullscreen", G_CALLBACK(webview_leave_fullscreen), this);
	
	return webview;
}

void Browser::create_new_tab(ustring address="", bool switch_to_last_tab = true) {
	
	if (notebook.get_n_pages() > LIMIT_TAB) return;
	set_title(app_name); 
	
	auto webview = create_new_webview();
	auto widget = manage(Glib::wrap(GTK_WIDGET(webview)));
	notebook.append_page(*widget, "New Tab");
	notebook.set_tab_reorderable(*widget, true);
	
	notebook.show_all();
	if (switch_to_last_tab) 
		jump_to_last_tab();
	
	if (!address.empty()) { 
		address_bar.set_text(address);
		webkit_web_view_load_uri(webview, address.c_str()); 
	}
}

void Browser::clone_current_tab(bool switch_to_last_tab = true) {
	
	if (notebook.get_n_pages() > LIMIT_TAB) return;
	
	auto current_webview = get_webview_from_current_tab();
	auto webview = create_new_webview();
	auto widget = manage(Glib::wrap(GTK_WIDGET(webview)));

	const gchar* title = webkit_web_view_get_title(current_webview); 
	if (title) {
		notebook.append_page(*widget, title);
		notebook.set_tab_reorderable(*widget, true);
		notebook.show_all();
		set_tab_label(*widget, title);
		
		if(switch_to_last_tab) jump_to_last_tab();
	}
	
	const gchar* address = webkit_web_view_get_uri(current_webview);
	if (address) {
		address_bar.set_text(address); 
		webkit_web_view_load_uri(webview, address);
	}
}

void Browser::on_search_next() {
	auto webview = get_webview_from_current_tab();
	auto controller = webkit_web_view_get_find_controller(webview);
	
	if(webkit_find_controller_get_search_text(controller))
		webkit_find_controller_search_next(controller);
}

void Browser::on_search_previous() {
	auto webview = get_webview_from_current_tab();
	auto controller = webkit_web_view_get_find_controller(webview);
	
	if(webkit_find_controller_get_search_text(controller))
		webkit_find_controller_search_previous(controller);	
}

void Browser::on_search_finish() {
	auto webview = get_webview_from_current_tab();
	auto controller = webkit_web_view_get_find_controller(webview);
	webkit_find_controller_search_finish(controller);
}

void Browser::on_search_text() {
	auto text = search_box_text.get_text();
	if (text.empty()) return;
	
	auto webview = get_webview_from_current_tab();
	auto controller = webkit_web_view_get_find_controller(webview);
	
	int match_count = 200;
	webkit_find_controller_search (controller, text.c_str(), WEBKIT_FIND_OPTIONS_CASE_INSENSITIVE, match_count);
}

void Browser::on_next() {
	auto webview = get_webview_from_current_tab();
	webkit_web_view_stop_loading(webview);
	
	webkit_web_view_go_forward(webview);
	
	auto title = webkit_web_view_get_title(webview);
	if(title)
		set_title(app_name + " - " + title);
}

void Browser::on_back() {
	auto webview = get_webview_from_current_tab();
	webkit_web_view_stop_loading(webview);
	
	webkit_web_view_go_back(webview);
	
	auto title = webkit_web_view_get_title(webview);
	if(title)
		set_title(app_name + " - " + title);
}

bool Browser::load_search_website(ustring address, WebKitWebView* webview) {
	if (address == "") return false;
	is_none_tab = false;
	
	find_and_replace(address, "+", "%2B");
	find_and_replace(address, "#", "%23");
	find_and_replace(address, " ", "+");
	find_and_replace(address, ":", "%3A");
	
	ustring uri = "https://www.google.com/search?q="; 
	uri.append(address);
	webkit_web_view_load_uri(webview, uri.c_str());
	
	return true;
}

bool Browser::load_website(ustring address, WebKitWebView* webview) {
	if (address == "") return false;
	is_none_tab = false;
	
	if (address.find("http") != 0) { address = "http://" + address; }
	webkit_web_view_load_uri(webview, address.c_str());
	
	return true;
}

void Browser::on_notebook_switch_page(Gtk::Widget* widget, guint tab_index)
{
	WebKitWebView* webview = (WebKitWebView*)Glib::unwrap(widget);
	const gchar* title = webkit_web_view_get_title(webview);
	const gchar* address = webkit_web_view_get_uri(webview);
	
	if (address) {
		check_address_bookmark(address);
		address_bar.set_text(address);
	}
	else {
		save_bookmark_button.set_label("♡");
		address_bar.set_text("");
	}
	
	if (title && *title) {
		set_title(app_name + " - " + title); 
		set_tab_label(*widget, title);
	}
	else {
		if (address) {
			remove_http_string(address);
			set_title(app_name + " - " + address);
			set_tab_label(*widget, address);
		}
	}
}

void Browser::on_sidebook_switch_page(Gtk::Widget* widget, guint tab_index)
{
	if(tab_index == 2) {
		// close
		showing_side_bar = false;
		browser_box_side_bar.hide();
	}
}

void Browser::jump_to_last_tab() {
	int last_tab = notebook.get_n_pages() - 1;
	notebook.set_current_page(last_tab);
	address_bar.grab_focus(); // focus to address bar
}

void Browser::toogle_search_box() {
	if (showing_search_box) {
		search_box.hide();
		showing_search_box = false;
		on_search_finish();
	}
	else {
		search_box.show();
		showing_search_box = true;
	}
}

enum { HISTORY, BOOKMARK };
void Browser::toogle_side_bar(int tab_index) {
	if (showing_side_bar) {
		int current_tab = sidebook.get_current_page();
		if(current_tab == tab_index) {
			browser_box_side_bar.hide();
			showing_side_bar = false;
		}
		else {
			sidebook.set_current_page(tab_index);
		}
	}
	else {
		browser_box_side_bar.show();
		sidebook.set_current_page(tab_index);
		showing_side_bar = true;
		
		if (tab_index == 0) {
			history_tree.get_selection()->unselect_all(); // unselect tree
			history_scroll.get_vadjustment()->set_value(0); // scroll to top
		}
		else {
			bookmark_tree.get_selection()->unselect_all(); // unselect tree
		}
		
	}
	list_address_selected.clear();
}


void Browser::delete_current_tab() {
	
	auto webview = get_webview_from_current_tab();
	
	auto context = get_web_context(webview);
	if (context == web_context) { switch_context = 0; }
	else { switch_context = 1; }
	
	webkit_web_view_stop_loading(webview); 
	
	int tab_index = notebook.get_current_page();
	notebook.remove_page(tab_index);
	
	if(notebook.get_n_pages() == 0) { create_new_tab(); is_none_tab = true; }
}

bool Browser::on_key_press(GdkEventKey* event)
{
	if(is_fullscreen) return false;
	
	switch(event->keyval) {
	case GDK_KEY_t:
		if (event->state & GDK_CONTROL_MASK ) {
			create_new_tab();
		}
		break;
	case GDK_KEY_f:
		if (event->state & GDK_CONTROL_MASK) {
			toogle_search_box();
			search_box_text.grab_focus();
		}
		break;
	case GDK_KEY_w:
		if (event->state & GDK_CONTROL_MASK) {
			delete_current_tab();
		}
		break;
	default:
		return false;
	}

	return true;
}

bool Browser::on_key_release(GdkEventKey* event)
{
	if (!is_fullscreen) {
		if (event->keyval == GDK_KEY_F2) {
			create_new_tab();
		}
		// delete tab
		else if (event->keyval == GDK_KEY_F3) {
			delete_current_tab();
		}
		else if (event->keyval == GDK_KEY_F5) {
			on_reload_site();
		}
		else if (event->keyval == GDK_KEY_F6) {
			clone_current_tab();
		}
		else if (event->keyval == GDK_KEY_F7) {
			toogle_side_bar(HISTORY);
		}
		else if (event->keyval == GDK_KEY_F8) {
			// open bookmark
			toogle_side_bar(BOOKMARK);
		}
		else if (event->keyval == GDK_KEY_Delete) {
			// delete history, bookmark
			if (history_tree.is_focus()) delete_history();
			else if (bookmark_tree.is_focus()) delete_bookmark();
		}
	}
	else {
		if (event->keyval == GDK_KEY_F11) {
			if (is_fullscreen) make_unfullscreen();
			else  make_fullscreen();
		}
	}
	
	return true;
} 

// press enter on entry
void Browser::on_enter_address() {
	ustring address = address_bar.get_text();
	if (address == "") return;
	
	set_title(app_name);

	auto webview = get_webview_from_current_tab();
	webkit_web_view_stop_loading(webview);
	
	std::string regex_str = R"(^((http[s]?|ftp)://)?\/?([^/\.]+\.)*?([^/\.]+\.[^:/\s\.]{2,3}(\.[^:/\s\.]‌​{2,3})?)(:\d+)?($|/)([^#?\s]+)?(.*?)?(#[\w\-]+)?$)";
	std::string regex_address = address;
	regex regex_url(regex_str);
	
	smatch base_match;
	
	if(regex_match(regex_address, base_match, regex_url)) {
		if (checkBlackListAddress(address)) {
			return;
		}
			
		load_website(address, webview);
	}
	else { 
		if (address.find("localhost") == 0 || address.find("http://localhost") == 0 || address.find("https://localhost") == 0
			|| address.find("127.0.0.1") == 0 || address.find("http://127.0.0.1") == 0 || address.find("https://127.0.0.1") == 0) 
		{ 
			load_website(address, webview); // valid url
		}
		else if(address.find("file://") == 0) {
			webkit_web_view_load_uri(webview, address.c_str());
		}
		else if(address.at(0) == '/') {
			string file = "file://" + address;
			webkit_web_view_load_uri(webview, file.c_str());
		}
		else if(address.find(" ") == 0) {
			load_search_website(address, webview); // invalid url 
		}
		else {
			load_search_website(address, webview); // invalid url
		}
	}
}

void Browser::set_address(ustring name) {
	address_bar.set_text(""); // clear text
	address_bar.set_text(name);
}

void Browser::set_tab_label(Widget& widget, ustring name) {
	
	ustring resize_name = name;
	resize_name.resize(TITLE_LENGTH);
	
	notebook.set_tab_label_text(widget, resize_name);
}

void Browser::set_tab_label(WebKitWebView* webview, ustring name) {
	
	ustring resize_name = name;
	resize_name.resize(TITLE_LENGTH);
	
	Widget* widget = manage(Glib::wrap(GTK_WIDGET(webview)));
	notebook.set_tab_label_text(*widget, resize_name);
}

int Browser::get_current_tab() {
	int result = notebook.get_current_page();
	if (result < 0) result = 0;
	return result;
}

/* HISTORY, BOOKMARK */
void Browser::save_history(ustring title, ustring address) {
	char* query = sqlite3_mprintf("INSERT INTO history(address, title, up) values ('%q', '%q', '%d');", 
	address.c_str(), title.c_str(), 0);
	int error = run_sql(ustring(query), NULL);
	if (error == SQLITE_OK) {
		
		load_history(title, address);
		
		add_history_tree(title, address, true);
		
		add_address_store(title, address); 
	}
	else {
		add_history_tree(title, address, true); 
  }
}

void Browser::save_bookmark(ustring title, ustring address) {
	if (title.empty() && address.empty()) return;
	
	// save to db
	
	// insert address
	char* query = sqlite3_mprintf("INSERT INTO bookmark(address, title) values ('%q', '%q');", 
		address.c_str(), title.c_str());
	int error = run_sql(ustring(query), NULL);
	
	sqlite3_free(query);
	
	// insert thanh cong
	if (error == SQLITE_OK) {		
	list_bookmark.insert(std::pair<ustring, ustring>(address, title)); 

		add_bookmark_tree(title, address, true);
		save_bookmark_button.set_label("♥");
		
		add_address_store(title, address);
	}
}

void Browser::on_save_bookmark() {	
	
	int tab_index = notebook.get_current_page();
	Widget* widget = notebook.get_nth_page(tab_index);
	WebKitWebView* webview = (WebKitWebView*)Glib::unwrap(widget);
	
	const gchar* title = webkit_web_view_get_title(webview);
	const gchar* address = webkit_web_view_get_uri(webview);
	
	if (title && address) {
		save_bookmark(title, address);
	}
}

void Browser::on_show_bookmark() {
	toogle_side_bar(BOOKMARK);
}

void Browser::check_address_bookmark(ustring address) {
	if (address.empty()) {
		save_bookmark_button.set_label("♡");
		return;
	}
	
	for(auto it = list_bookmark.cbegin(); it != list_bookmark.cend();) {
		if (it->first.compare(address) == 0) {
			save_bookmark_button.set_label("♥"); 
			return;
		}
		else
			++it;
	}
	
	save_bookmark_button.set_label("♡"); 
}
void Browser::on_show_history() {
	toogle_side_bar(HISTORY);
}

/* DB */
void Browser::load_history(ustring title, ustring address) {
	//~ tuple<ustring, ustring> tmp(title, address);
	//~ list_history.push_front(tmp);
}

void Browser::load_bookmark(ustring title, ustring address) {
	list_bookmark.insert(std::pair<ustring, ustring>(address, title)); 
}

void Browser::add_history_tree(ustring title, ustring address, bool prepend = false) {
	TreeModel::iterator iter;
	if(prepend) iter = history_store->prepend();
	else iter = history_store->append();
	
	TreeModel::Row row = *iter;
	row[tree_model.column_title] = title;	
	row[tree_model.column_address] = address;
}

void Browser::add_bookmark_tree(ustring title, ustring address, bool prepend = false) {
	TreeModel::iterator iter;
	if(prepend) iter = bookmark_store->prepend();
	else iter = bookmark_store->append();
	
	TreeModel::Row row = *iter;
	row[tree_model.column_title] = title;	
	row[tree_model.column_address] = address;
}

void Browser::add_address_store(ustring title, ustring address) {
	cout << " add addresss store : " << title << " - " << address << endl;
	//~ TreeModel::iterator iter = address_store->prepend();
	//~ TreeModel::Row row = *iter;
	//~ row[address_model_column.column_name] = title;	
	
	//~ iter = address_store->prepend(row.children());
	//~ TreeModel::Row childrow = *iter;
	//~ childrow[address_model_column.column_name] = address;
	
	//~ TreeModel::iterator iter = address_store->prepend();
	//~ TreeModel::Row row = *iter;
	//~ row[address_model_column.column_name] = title;	
	//~ row[address_model_column.column_address] = address;	
}

static int load_all_bookmark(void* thisclass, int argc, char** argv, char** col) {
	Browser* browser = (Browser*)thisclass;
	ustring title, address;
	
	for(int i = 0; i < argc; i++) {	
		if (i == 0) address = ustring(argv[i]);
		else if (i == 1) title = ustring(argv[i]);
	}
	
	if (title != "" && address != "") {
		browser->load_bookmark(title, address);
		browser->add_bookmark_tree(title, address);
		
		browser->add_address_store(title, address);
	}
		
	return 0;
}

static int load_all_history(void* thisclass, int argc, char** argv, char** col) {
	
	Browser* browser = (Browser*)thisclass;
	ustring title, address;
	
	for(int i = 0; i < argc; i++) {	
		if (i == 0) address = ustring(argv[i]);
		else if (i == 1) title = ustring(argv[i]);
	}
	
	if (title != "" && address != "") {
		browser->load_history(title, address);
		browser->add_history_tree(title, address);
		
		browser->add_address_store(title, address);
	}
		
	return 0;
}

int Browser::run_sql(ustring sql, int(*func)(void*, int, char**, char**)) {
	ustring db_path = exe_path + "/data.db";
	
	sqlite3 *db;
	int error = sqlite3_open(db_path.c_str(), &db);
	if (error) {
		sqlite3_close(db);
		return error;
	}
	
	char* errmsg = 0;
	error = sqlite3_exec(db, sql.c_str(), func, this, &errmsg);
	if (error != SQLITE_OK) {
		sqlite3_free(errmsg);
	}
	sqlite3_close(db);
	return error;
}

void Browser::on_bookmark_tree_double_click(const TreeModel::Path& path, TreeViewColumn* column) {
	TreeModel::iterator iter = bookmark_store->get_iter(path);
	if (iter) {
		TreeModel::Row row = *iter;
		ustring address = row[tree_model.column_address];
		
		if (is_none_tab) {
			is_none_tab = false;
			
			if(address.find("file://") == 0) {
				webkit_web_view_load_uri(get_webview_from_current_tab(), address.c_str());
			}
			else if(address.at(0) == '/') {
				string file = "file://" + address;
				webkit_web_view_load_uri(get_webview_from_current_tab(), file.c_str());
			}
			else {
				load_website(address, get_webview_from_current_tab()); 
			}
		}
		else create_new_tab(address);
	}
}

void Browser::on_history_tree_double_click(const TreeModel::Path& path, TreeViewColumn* column) {
	TreeModel::iterator iter = history_store->get_iter(path);
	if (iter) {
		TreeModel::Row row = *iter;
		ustring address = row[tree_model.column_address];
		
		if (is_none_tab) {
			is_none_tab = false;
			
			if(address.find("file://") == 0) {
				webkit_web_view_load_uri(get_webview_from_current_tab(), address.c_str());
			}
			// check day co phai la local file ko
			else if(address.at(0) == '/') {
				string file = "file://" + address;
				webkit_web_view_load_uri(get_webview_from_current_tab(), file.c_str());
			}
			else {
				load_website(address, get_webview_from_current_tab()); 
			}
		}
		else create_new_tab(address);
	}
}

void Browser::on_reload_site() {
	auto webview = get_webview_from_current_tab();
	webkit_web_view_stop_loading(webview); // stop loading webview hien tai neu dang chay
	webkit_web_view_reload(webview); // reload
}

void Browser::on_completion_action_activated(int index) {
	action_map::iterator iter = completion_action.find(index);
	if(iter != completion_action.end()) {
		Glib::ustring title = iter->second;
	}
}

bool Browser::on_completion_match(const Glib::ustring& key, const Gtk::TreeModel::const_iterator& iter) {
	if(iter) {

		Glib::ustring::size_type key_length = key.size();
		if (key_length <= 1) return false; // khong search text chi co 1 chu hoac khong
		
		Gtk::TreeModel::Row row = *iter;
		Glib::ustring filter_string = row[address_model_column.column_name];

		Glib::ustring filter_string_start = filter_string.substr(0, key_length);
		filter_string_start = filter_string_start.lowercase(); // lower case de tim kiem duoc nhieu hon

		if(key == filter_string_start) {
			return true; //A match was found.
		}
	}

  return false; //No match.
}

void Browser::delete_history() {
	
	if (list_address_selected.size() <= 0) return;
	
	auto model = history_tree.get_model();
	auto paths = history_tree.get_selection()->get_selected_rows();
	
	while(!paths.empty()) {
		auto path = paths.back();
		auto iter = model->get_iter(path);
		history_store->erase(iter);
		paths.pop_back(); // remove last element from paths;
	}
	history_tree.get_selection()->unselect_all();
	history_scroll.get_vadjustment()->set_value(0); // scroll to top
	
	// remove duplicate element, sort first
	sort(list_address_selected.begin(), list_address_selected.end());
	list_address_selected.erase(unique(list_address_selected.begin(), list_address_selected.end() ), list_address_selected.end());
	
	string sql = "DELETE FROM history WHERE address IN (";
	for(auto& x : list_address_selected) {
		sql.append("'");
		sql.append(x.c_str());
		sql.append("',");
	}
	sql.pop_back();
	sql.append(");");
	run_sql(sql, NULL);
}

void Browser::delete_bookmark() {
	if (list_address_selected.size() <= 0) return;
	
	auto model = bookmark_tree.get_model();
	auto paths = bookmark_tree.get_selection()->get_selected_rows();
	
	while(!paths.empty()) {
		auto path = paths.back();
		auto iter = model->get_iter(path);
		bookmark_store->erase(iter);
		paths.pop_back(); // remove last element from paths;
	}
	bookmark_tree.get_selection()->unselect_all();
	
	string sql = "DELETE FROM bookmark WHERE address IN (";
	for(auto& x : list_address_selected) {
		sql.append("'");
		sql.append(x.c_str());
		sql.append("',");
	}
	sql.pop_back();
	sql.append(");");
	run_sql(sql, NULL);
	
	// delete from list bookmark
	for(auto it = list_bookmark.begin(); it != list_bookmark.end();) {
		for(auto& x : list_address_selected) {
			if (it->first.compare(x) == 0)
				it = list_bookmark.erase(it);
			else
				++it;
		}
	}
	
	auto webview = get_webview_from_current_tab();
	auto address = webkit_web_view_get_uri(webview);
	if (address) check_address_bookmark(address);
	
}

void Browser::on_history_tree_focus()
{
	list_address_selected.clear(); // clear select paths before for each
	history_tree.get_selection()->selected_foreach_iter(sigc::mem_fun(*this, &Browser::on_history_tree_selected));
}

void Browser::on_bookmark_tree_focus()
{
	list_address_selected.clear(); // clear select paths before for each
	bookmark_tree.get_selection()->selected_foreach_iter(sigc::mem_fun(*this, &Browser::on_bookmark_tree_selected));
}

void Browser::on_history_tree_selected(const TreeModel::iterator& iter) {
	TreeModel::Row row = *iter;
	list_address_selected.push_back(row[tree_model.column_address]);
}

void Browser::on_bookmark_tree_selected(const TreeModel::iterator& iter) {
	TreeModel::Row row = *iter;
	list_address_selected.push_back(row[tree_model.column_address]);
}

Browser::Browser(ustring path) 
	: exe_path(path), blacklist(path)
{
	/* WINDOW */
	set_title("DBT Browser");
	set_border_width(0);
	set_default_size(1024, 600);
	
	maximize();
	
	add(main_box);
	main_box.pack_start(menu_box, PACK_SHRINK);
	main_box.pack_start(browser_box, PACK_EXPAND_WIDGET);
	main_box.pack_start(search_box, PACK_SHRINK, 2);
	
	menu_box.pack_start(back_button, PACK_SHRINK);
	menu_box.pack_start(next_button, PACK_SHRINK);
	menu_box.pack_start(address_bar, PACK_EXPAND_WIDGET);
	menu_box.pack_start(reload_button, PACK_SHRINK);
	menu_box.pack_start(save_bookmark_button, PACK_SHRINK);
	menu_box.pack_start(history_button, PACK_SHRINK);
	menu_box.pack_start(bookmark_button, PACK_SHRINK);
	
	browser_box.pack_start(browser_box_side_bar, PACK_SHRINK);
	browser_box.pack_start(notebook, PACK_EXPAND_WIDGET);
	
	search_box.pack_end(search_box_close_button, PACK_SHRINK, 16);
	search_box.pack_end(search_box_matches, PACK_SHRINK, 12);
	search_box.pack_end(search_box_highlight_button, PACK_SHRINK, 12);
	search_box.pack_end(search_box_down_button, PACK_SHRINK);
	search_box.pack_end(search_box_up_button, PACK_SHRINK);
	search_box.pack_end(search_box_text, PACK_SHRINK);
	search_box.pack_end(search_title, PACK_SHRINK, 8);
	
	browser_box_side_bar.pack_start(sidebook, PACK_EXPAND_WIDGET);
	
	// search box
	search_title.set_text("Search: ");
	search_box_up_button.set_label("▲");
	search_box_down_button.set_label("▼");
	search_box_highlight_button.set_label("Hightlight");
	search_box_matches.set_text("Matches");
	search_box_close_button.set_label("✖");
	
	// scroll bar;
	history_scroll.add(history_tree);
	bookmark_scroll.add(bookmark_tree);
	
	sidebook.append_page(history_scroll, "History");
	sidebook.append_page(bookmark_scroll, "Bookmark");
	sidebook.set_tab_reorderable(history_scroll, true);
	sidebook.set_tab_reorderable(bookmark_scroll, true);
	sidebook.append_page(close_side_bar, "x");
	
	/* MENU BUTTON */
	back_button.set_label("◁");
	next_button.set_label("▷");
	save_bookmark_button.set_label("♡");
	history_button.set_label("History");
	bookmark_button.set_label("Bookmark");
	reload_button.set_label("↻");
	
	/* NOTE BOOK */
	notebook.set_scrollable(true);
	
	/* SIDE BOOK */
	sidebook.set_scrollable(true);
	sidebook.set_size_request(360, -1);
	
	/* HISTORY, BOOKMARK TREE */
	history_scroll.set_border_width(4);
	history_scroll.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
	
	bookmark_scroll.set_border_width(4);
	bookmark_scroll.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
	
	history_store = ListStore::create(tree_model);
	history_tree.set_model(history_store);
	history_tree.append_column("Title", tree_model.column_title);
	history_tree.append_column("Address", tree_model.column_address);
	history_tree.set_headers_visible(false);
	history_tree.get_column(0)->set_min_width(160);
	history_tree.get_column(0)->set_max_width(480);
	
	bookmark_store = ListStore::create(tree_model);
	bookmark_tree.set_model(bookmark_store);
	bookmark_tree.append_column("Title", tree_model.column_title);
	bookmark_tree.append_column("Address", tree_model.column_address);
	bookmark_tree.set_headers_visible(false);
	bookmark_tree.get_column(0)->set_min_width(160);
	bookmark_tree.get_column(0)->set_max_width(480);

	/* CONNECT FUNCTION */
	next_button.signal_clicked().connect(sigc::mem_fun(*this, &Browser::on_next));
	back_button.signal_clicked().connect(sigc::mem_fun(*this, &Browser::on_back));
	save_bookmark_button.signal_clicked().connect(sigc::mem_fun(*this, &Browser::on_save_bookmark));
	history_button.signal_clicked().connect(sigc::mem_fun(*this, &Browser::on_show_history));
	bookmark_button.signal_clicked().connect(sigc::mem_fun(*this, &Browser::on_show_bookmark));
	reload_button.signal_clicked().connect(sigc::mem_fun(*this, &Browser::on_reload_site));
	
	address_bar.signal_activate().connect(sigc::mem_fun(*this, &Browser::on_enter_address));
	notebook.signal_switch_page().connect(sigc::mem_fun(*this, &Browser::on_notebook_switch_page));
	sidebook.signal_switch_page().connect(sigc::mem_fun(*this, &Browser::on_sidebook_switch_page));
	
	history_tree.signal_row_activated().connect(sigc::mem_fun(*this, &Browser::on_history_tree_double_click));
	history_tree.get_selection()->set_mode(Gtk::SELECTION_MULTIPLE);
	history_tree.signal_grab_focus().connect(sigc::mem_fun(*this, &Browser::on_history_tree_focus));
	
	bookmark_tree.signal_row_activated().connect(sigc::mem_fun(*this, &Browser::on_bookmark_tree_double_click));
	bookmark_tree.get_selection()->set_mode(Gtk::SELECTION_MULTIPLE);
	bookmark_tree.signal_grab_focus().connect(sigc::mem_fun(*this, &Browser::on_bookmark_tree_focus));
	
	search_box_down_button.signal_clicked().connect(sigc::mem_fun(*this, &Browser::on_search_next));
	search_box_up_button.signal_clicked().connect(sigc::mem_fun(*this, &Browser::on_search_previous));
	search_box_text.signal_activate().connect(sigc::mem_fun(*this, &Browser::on_search_text));
	search_box_close_button.signal_clicked().connect(sigc::mem_fun(*this, &Browser::toogle_search_box));
	
	this->add_events( Gdk::KEY_PRESS_MASK); 
	this->signal_key_release_event().connect(sigc::mem_fun(*this, &Browser::on_key_release)); 
	this->signal_key_press_event().connect(sigc::mem_fun(*this, &Browser::on_key_press));
	
	show_all_children();
	
	web_data_manager = webkit_website_data_manager_new((exe_path + "/data/data_base").c_str(), (exe_path + "/data/data_cache").c_str(), 
		(exe_path + "/data/local_storage").c_str(), (exe_path + "/data/disk_cache").c_str(), (exe_path + "/data/offline_cache").c_str(), 
		(exe_path + "/data/index").c_str(), (exe_path + "/data/websql").c_str());
	

	// context 1
	web_context = webkit_web_context_new_with_website_data_manager(web_data_manager);
	webkit_web_context_set_cache_model(web_context, WEBKIT_CACHE_MODEL_WEB_BROWSER);
	webkit_web_context_set_process_model(web_context, WEBKIT_PROCESS_MODEL_MULTIPLE_SECONDARY_PROCESSES);
	webkit_web_context_set_web_process_count_limit(web_context, LIMIT_TAB);
	// varible
	switch_context = 0;
	is_fullscreen = false;
	is_none_tab = true;

	// cookie 1
	auto cookie_path = exe_path + "/cookie";
	auto cookie_manager = webkit_web_context_get_cookie_manager (web_context);
	webkit_cookie_manager_set_accept_policy(cookie_manager, WEBKIT_COOKIE_POLICY_ACCEPT_ALWAYS);
	webkit_cookie_manager_set_persistent_storage(cookie_manager, cookie_path.c_str(), WEBKIT_COOKIE_PERSISTENT_STORAGE_TEXT);
	
	create_new_tab();
	
	/* SIDE BAR */
	browser_box_side_bar.hide();
	showing_side_bar = false;
	
	/* SEARCH BOX */
	search_box.hide();
	showing_search_box = false;
	
	/* CLIPBOARD */
	
	/* ENTRY COMPLETION */
	address_completion = EntryCompletion::create();
	address_bar.set_completion(address_completion);
	
	address_store = TreeStore::create(address_model_column);
	address_completion->set_model(address_store);
	address_completion->set_text_column(address_model_column.column_address);

	/* DB */
	run_sql("select * from bookmark order by datetime(time) DESC", load_all_bookmark);
	run_sql("select * from history order by datetime(time) DESC", load_all_history);
}

