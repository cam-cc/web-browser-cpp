#include "browser.h"
#include <limits.h>
#include <unistd.h>
#include <fstream>

int main( int argc, char **argv) {
	try {
		char result[PATH_MAX];
		ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
		string exe_path = string(result, (count > 0) ? count : 0);
		exe_path = fs::path(exe_path).parent_path().string();
		
		Glib::RefPtr<Gtk::Application> app = Gtk::Application::create(argc, argv, "dbt.tbrowser");
		Browser browser(exe_path);
		
		app->run(browser);
	}
	catch(exception const& e) {
		std::ofstream log("error.txt", std::ios_base::out | std::ios_base::app);
		log << e.what() << std::endl;
	}
}
