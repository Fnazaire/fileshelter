/*
 * Copyright (C) 2016 Emeric Poupon
 *
 * This file is part of fileshelter.
 *
 * fileshelter is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * fileshelter is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with fileshelter.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <iostream>
#include <boost/property_tree/xml_parser.hpp>

#include <Wt/WServer>

#include "utils/Config.hpp"
#include "utils/Logger.hpp"

#include "database/DbHandler.hpp"
#include "database/DbCleaner.hpp"

#include "ui/FileShelterApplication.hpp"

std::vector<std::string> generateWtConfig(std::string execPath)
{
	std::vector<std::string> args;

	boost::filesystem::path wtConfigPath = Config::instance().getPath("working-dir") / "wt_config.xml";

	args.push_back(execPath);
	args.push_back("--config=" + wtConfigPath.string());
	args.push_back("--docroot=" + Config::instance().getString("docroot"));
	args.push_back("--approot=" + Config::instance().getString("approot"));

	if (Config::instance().getBool("tls-enable", false))
	{
		args.push_back("--https-port=" + std::to_string( Config::instance().getULong("listen-port", 5081)));
		args.push_back("--https-address=" + Config::instance().getString("listen-addr", "0.0.0.0"));
		args.push_back("--ssl-certificate=" + Config::instance().getString("tls-cert"));
		args.push_back("--ssl-private-key=" + Config::instance().getString("tls-key"));
		args.push_back("--ssl-tmp-dh=" + Config::instance().getString("tls-dh"));
	}
	else
	{
		args.push_back("--http-port=" + std::to_string( Config::instance().getULong("listen-port", 5081)));
		args.push_back("--http-address=" + Config::instance().getString("listen-addr", "0.0.0.0"));
	}

	// Generate the wt_config.xml file
	boost::property_tree::ptree pt;

	pt.put("server.application-settings.<xmlattr>.location", "*");
	pt.put("server.application-settings.log-file", Config::instance().getPath("log-file"));
	pt.put("server.application-settings.max-request-size", Config::instance().getULong("max-file-size", 100) * 1024 /* kB */);
	pt.put("server.application-settings.behind-reverse-proxy", Config::instance().getBool("behind-reverse-proxy", false));
	pt.put("server.application-settings.progressive-bootstrap", true);

	std::ofstream oss(wtConfigPath.string().c_str(), std::ios::out);
	boost::property_tree::xml_parser::write_xml(oss, pt);

	return args;
}

int main(int argc, char *argv[])
{

	boost::filesystem::path configFilePath = "/etc/fileshelter.conf";
	int res = EXIT_FAILURE;

	assert(argc > 0);
	assert(argv[0] != NULL);

	if (argc >= 2)
		configFilePath = std::string(argv[1], 0, 256);

	try
	{
		Config::instance().setFile(configFilePath);

		// Make sure working directory exists
		// TODO check with boost::system::error_code ec;
		boost::filesystem::create_directories(Config::instance().getPath("working-dir") / "files");

		// Recreate the tmp directory in order to flush it
		auto tmpDir = Config::instance().getPath("working-dir") / "tmp";
		boost::filesystem::remove_all(tmpDir);
		boost::filesystem::create_directories(tmpDir);

		// Set the WT_TMP_DIR inside the working dir, used to upload files
		setenv("WT_TMP_DIR", tmpDir.string().c_str(), 1);

		// Construct WT configuration and get the argc/argv back
		std::vector<std::string> wtServerArgs = generateWtConfig(argv[0]);

		const char* wtArgv[wtServerArgs.size()];
		for (std::size_t i = 0; i < wtServerArgs.size(); ++i)
		{
			std::cout << "ARG = " << wtServerArgs[i] << std::endl;
			wtArgv[i] = wtServerArgs[i].c_str();
		}

		Wt::WServer server(argv[0]);
		server.setServerConfiguration (wtServerArgs.size(), const_cast<char**>(wtArgv));

		Wt::WServer::instance()->logger().configure("*"); // log everything, TODO configure this?

		// Initializing a connection pool to the database that will be shared along services
		std::unique_ptr<Wt::Dbo::SqlConnectionPool>
			connectionPool( Database::Handler::createConnectionPool(Config::instance().getPath("working-dir") / "fileshelter.db"));

		Database::Cleaner dbCleaner(*connectionPool);

		// bind entry point
		server.addEntryPoint(Wt::Application, boost::bind(UserInterface::FileShelterApplication::create,
					_1, boost::ref(*connectionPool)));

		// Start
		FS_LOG(MAIN, INFO) << "Starting database cleaner...";
		dbCleaner.start();

		FS_LOG(MAIN, INFO) << "Starting server...";
		server.start();

		// Wait
		FS_LOG(MAIN, INFO) << "Now running...";
		Wt::WServer::waitForShutdown(argv[0]);

		// Stop
		FS_LOG(MAIN, INFO) << "Stopping server...";
		server.stop();

		FS_LOG(MAIN, INFO) << "Stopping database cleaner...";
		dbCleaner.stop();

		res = EXIT_SUCCESS;
	}
	catch( libconfig::FileIOException& e)
	{
		std::cerr << "Cannot open config file " << configFilePath << std::endl;
	}
	catch( libconfig::ParseException& e)
	{
		std::cerr << "Caught libconfig::ParseException! error='" << e.getError() << "', file = '" << e.getFile() << "', line = " << e.getLine() << std::endl;
	}
	catch(Wt::WServer::Exception& e)
	{
		std::cerr << "Caught a WServer::Exception: " << e.what() << std::endl;
	}
	catch(std::exception& e)
	{
		std::cerr << "Caught std::exception: " << e.what() << std::endl;
	}

	return res;
}

