/*
Author: Avianco Technologies Pvt Ltd
#51, Chourasia Shreyas Complex,
4th Floor, 3rd Cross, Ashwanth Nagar, Marathahalli, Bangalore,
Karnataka-560037, India
E-mail: avianco.in@gmail.com

Purpose: This example client program is to demonostrate how to implement 
Avianco UTM Platform API calls in a C/C++ based client program.
Drone Manufacturers and Solution providers may use this as an example
 to implement the UTM API in their drone flight controller software or
 Ground Control Software modules.

 Dependencies: This example program is created using Visual Studio 2017 
 and relies on cpprestsdk and cereal libraries.
 cpprestsdk - Refer to  https://github.com/microsoft/cpprestsdk
 cereal serialization library - Refer to https://uscilab.github.io/cereal/

*/

#include <string>
#include <codecvt>
#include "stdafx.h"

#include <cpprest/http_listener.h>              // HTTP server
#include <cpprest/json.h>                       // JSON library
#include <cpprest/uri.h>                        // URI library
#include <cpprest/ws_client.h>                  // WebSocket client
#include <cpprest/containerstream.h>            // Async streams backed by STL containers
#include <cpprest/interopstream.h>              // Bridges for integrating Async streams with STL and WinRT streams
#include <cpprest/rawptrstream.h>               // Async streams backed by raw pointer to memory
#include <cpprest/producerconsumerstream.h>     // Async streams for producer consumer scenar
#include <sstream>
#include <locale>
#include <cpprest/http_client.h>
#include <cereal/types/unordered_map.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/archives/binary.hpp>
#include <fstream>
#include "cpprest/filestream.h"
#include "cpprest/producerconsumerstream.h"
#include <iostream>



typedef web::json::value JsonValue;
typedef web::json::value::value_type JsonValueType;
typedef std::wstring String;
typedef std::wstringstream StringStream;

using namespace ::pplx;
using namespace utility;
using namespace concurrency::streams;
using namespace web;
using namespace web::http;
using namespace web::http::client;
using namespace web::json;
using namespace std;
using namespace utility;                    // Common utilities like string conversions
using namespace web;                        // Common features like URIs.
using namespace web::http;                  // Common HTTP functionality
using namespace web::http::client;          // HTTP client features
using namespace concurrency::streams;       // Asynchronous streams
using namespace utility::conversions;
using namespace web::http::client;

utility::string_t registered_email = L"xxxxxxxx@xmail.com"; // Replace the email with your registered email
utility::string_t registered_password = L"password"; //// Replace the password with your login password
utility::string_t api_key = L"XXXXXXX-XXXXXXX-XXXXXXX-XXXXXXX"; // Replace this with your Developer API key


// Helper to construct login json
JsonValue CreateJSONObject();

// Helper to construct position data json every cycle
JsonValue CreateFlightPositionJSONObject(int i);

// Helpers to parse a given sample json file that has track data
// In this example we use this position data to simulate reporting drone realtime position
std::string ParseJSONValueRecursive(web::json::value v);
void parseJSONFile(string_t file);

// Example shows how to construct and post loginUser API call
void PostLogin();

// Example shows how to construct and post updateFlightPlan API call with flight status to TakeoffEvent
void PostStartFlight();

// Example shows how to construct and post submitLiveData API call with live position data
void PostFligtLiveData(int i);

// Example shows how to construct and post updateFlightPlan API call with flight status to LandingEvent
void PostEndFlight();

utility::string_t m_szSessionToken = L"";
static int number_of_coords = 0;
char arrayOfLatCoords[100][256];
char arrayOfLonCoords[100][256];
static bool logged_in = false;
char flightID[256];

int main(int argc, char** argv)
{
	if (argc < 2 || argc > 2)
	{
		std::cout << "Invalid arguments:" << std::endl;
		std::cout << "Usage: UTMClient.exe trackfilename.json" << std::endl;
		getchar();
		return 0;
	}  

	utility::string_t filename = utility::conversions::to_string_t(argv[1]);
	parseJSONFile(filename);
	for (int i = 0; i<number_of_coords; i++) {
		printf("%s", arrayOfLatCoords[i]);
		printf(",");
		printf("%s\n", arrayOfLonCoords[i]);

	}

	std::cout << "Logging in......" << std::endl;

	PostLogin();
	
	if(logged_in && !m_szSessionToken.empty())
	{
	std::cout << "Start of Flight" << std::endl;

	PostStartFlight();

	// Now generate post request using key and token to report flight data
	std::cout << "Start reporting live position" << std::endl;
	for (int i = 0; i < number_of_coords; i++) {
		PostFligtLiveData(i);
		Sleep(5000);
	}

	std::cout << "End of Flight" << std::endl;

	PostEndFlight();
	}
	else
	{
		std::cout << "Log in failed" << std::endl;
	}
}

void readFlightDataFromFile(string_t importFile)
{
	try {		
		ifstream_t      f(importFile);                              // filestream of working file
		stringstream_t  s;                                          // string stream for holding JSON read from file
		json::value     v;                                          // JSON read from input file
		
		if(f) 
		{
			s << f.rdbuf();                                         // stream results of reading from file stream into string stream
			f.close();                                              // close the filestream

			v = json::value::parse(s);  //v.parse(s);                                             // parse the resultant string stream.
			ParseJSONValueRecursive(v);
		}
	}
	catch (web::json::json_exception excep) 
	{
		std::cout << "ERROR Parsing JSON: ";
		std::cout << excep.what();		
	}
}

void DisplayJSONValue(json::value v)
{
	if (!v.is_null())
	{
		// Loop over each element in the object		
		for (const auto &pr : v.as_object()) {
			std::wcout << L"String: " << pr.first << L", Value: " << pr.second << endl;
			std::string key = utility::conversions::to_utf8string(pr.first);
			if (key.compare("token") == 0)
			{
				m_szSessionToken = pr.second.serialize();
				// remove double quotes at the beginning and at the end of token
				m_szSessionToken.erase(
					remove(m_szSessionToken.begin(), m_szSessionToken.end(), '\"'),
					m_szSessionToken.end()
				);

			}
		}
		
	}
}

// Construct the login json header
JsonValue CreateJSONObject()
{
	// Create a JSON object.
	json::value ep;
	ep[L"email"] = json::value::string(registered_email);
	ep[L"password"] = json::value::string(registered_password);
	json::value ConVal;
	ConVal[L"ConVal"] = json::value::array({ ep });
	return ep;
}

std::string ParseJSONValueRecursive(web::json::value v)
{
	std::stringstream ss;
	try
	{
		if (!v.is_null())
		{
			if (v.is_object())
			{
				// Loop over each element in the object
				for (auto iter = v.as_object().cbegin(); iter != v.as_object().cend(); ++iter)
				{
					// It is necessary to make sure that you get the value as const reference
					// in order to avoid copying the whole JSON value recursively (too expensive for nested objects)
					const utility::string_t &str = iter->first;
					const web::json::value &value = iter->second;

					if (value.is_object() || value.is_array())
					{
						ss << "Parent: " << utility::conversions::to_utf8string(str) << std::endl;

						ss << ParseJSONValueRecursive(value);

						ss << "End of Parent: " << utility::conversions::to_utf8string(str) << std::endl;
					}
					else
					{
						ss << "str: " << utility::conversions::to_utf8string(str) << ", Value: " << utility::conversions::to_utf8string(value.serialize()) << std::endl;
						std::string key = utility::conversions::to_utf8string(str);
						if (key.compare("latitude") == 0)
						{
							std::string lat = utility::conversions::to_utf8string(value.serialize());
							memcpy((void *) &arrayOfLatCoords[number_of_coords - 1][0], lat.c_str(), lat.length());
						}
						if (key.compare("longitude") == 0)
						{
							std::string lon = utility::conversions::to_utf8string(value.serialize());
							memcpy((void *) &arrayOfLonCoords[number_of_coords - 1][0], lon.c_str(), lon.length());
						}
						if (key.compare("flightId") == 0)
						{
							std::string flightId = utility::conversions::to_utf8string(value.serialize());
							flightId.erase(
								remove(flightId.begin(), flightId.end(), '\"'),
								flightId.end()
							);
							memcpy((void *)&flightID[0], flightId.c_str(), flightId.length());
						}

					}
				}
			}
			else if (v.is_array())
			{
				// Loop over each element in the array
				for (size_t index = 0; index < v.as_array().size(); ++index)
				{
					const web::json::value &value = v.as_array().at(index);

					ss << "Array: " << index << std::endl;
					number_of_coords++;
					ss << ParseJSONValueRecursive(value);
				}
			}
			else
			{
				ss << L"Value: " << utility::conversions::to_utf8string(v.serialize()) << std::endl;
			}
		}
	}
	catch (const std::exception& e)
	{
		std::cout << e.what() << std::endl;		
	}

	return ss.str();
}

void parseJSONFile(string_t file)
{
	try {		
		ifstream_t      f(file);                              // filestream of working file
		stringstream_t  s;                                    // string stream for holding JSON read from file
		json::value     v;                                    // JSON read from input file																
		if (f)
		{
			s << f.rdbuf();                                         // stream results of reading from file stream into string stream
			f.close();                                              // close the filestream

			v = json::value::parse(s);                             // parse the resultant string stream.
			std::cout << ParseJSONValueRecursive(v);
		}
	}
	catch (web::json::json_exception excep)
	{
		std::cout << "ERROR Parsing JSON: ";
		std::cout << excep.what();		
	}
}

JsonValue CreateFlightPositionJSONObject(int i)
{

	json::value positionData;
	
	positionData[L"altitude"] = json::value::number(123);
	positionData[L"created_date"] = json::value::string(L"019-05-17T15:54:22+0530");

	std::string flight_id(flightID);
	utility::string_t flight_id_converted = utility::conversions::to_string_t(flight_id);

    positionData[L"flight_plan_id"] = json::value::string(flight_id_converted);


	positionData[L"heading"] = json::value::string(L"160");
	positionData[L"horizontal_velocity"] = json::value::number(-34.397);
	
	float lat_val = std::stof(arrayOfLatCoords[i]);
	float lon_val = std::stof(arrayOfLonCoords[i]);

	positionData[L"latitude"] = json::value::number(lat_val);
	positionData[L"longitude"] = json::value::number(lon_val); 
	utility::datetime time_now = utility::datetime::utc_now();
	utility::string_t time_stamp = time_now.to_string();
	positionData[L"time_stamp"] = json::value::string(time_stamp);
	positionData[L"traffic_source_type"] = json::value::string(L"3G");
	positionData[L"uin_id"] = json::value::number(12121);
	positionData[L"vertical_velocity"] = json::value::number(150.644);
	return positionData;
}

void PostLogin()
{
	json::value postData;

	postData = CreateJSONObject();

	http_client_config config;
	config.set_validate_certificates(false);

	credentials creds(registered_email, registered_password);

	config.set_credentials(creds);

	http_client client(U("https://utm.avianco.in/v1/users/loginUser"), config);

	http_request req(methods::POST);
	req.headers().add(L"Content-Type", L"application/json");
	
	req.headers().add(L"x-api-key", api_key);
	if (!m_szSessionToken.empty())
		req.headers().add(L"Cookie", m_szSessionToken);

	req.headers().set_content_type(U("application/json"));
	req.set_body(postData);
	try
	{
		
		client.request(req).then([=](web::http::http_response response)-> pplx::task<json::value>{
			// Your code here
			std::wcout << response.status_code() << std::endl;

			if (response.status_code() == status_codes::OK)
			{
				//auto body = response.extract_string();

				logged_in = true;

				auto headers = response.headers();

				auto it = headers.find(U("Set-Cookie"));

				if (it != headers.end()) {
					m_szSessionToken = it->second;
				}
				response.headers().set_content_type(L"application/json");

				return response.extract_json();
			}
		}).then([=](pplx::task<json::value> previous_task) mutable {

			// Get the JSON value from the task and call the DisplayJSONValue method
			try
			{
				json::value const & value = previous_task.get();
				DisplayJSONValue(value);
			}
			catch (http_exception const & e)
			{
				std::wcout << e.what() << std::endl;
			}

			if (previous_task._GetImpl()->_HasUserException()) {
				auto holder = previous_task._GetImpl()->_GetExceptionHolder(); // Probably should put in try

				try {
					// Need to make sure you try/catch here, as _RethrowUserException can throw
					holder->_RethrowUserException();
				}
				catch (std::exception& e) {
					// Do what you need to do here
					std::wcout << e.what();
				}
			}
		})
		.wait();


		
	}
	catch (std::exception& e)
	{
		std::wcout << e.what();
	}

	//getchar();
}

void PostStartFlight()
{
	json::value postData;
	std::string flight_id(flightID);
	utility::string_t flight_id_converted = utility::conversions::to_string_t(flight_id);
	postData[L"flight_plan_id"] = json::value::string(flight_id_converted);
	postData[L"status"] = json::value::number(5);

	http_client_config config;
	config.set_validate_certificates(false);
	http_client client(U("https://utm.avianco.in/v1/flight/updateFlightPlan"), config);

	http_request req(methods::POST);
	req.headers().add(L"Content-Type", L"application/json");

	req.headers().add(L"x-api-key", api_key);
	req.headers().add(L"x-access-token", m_szSessionToken);
	req.headers().set_content_type(U("application/json"));
	req.set_body(postData);
	try
	{

		client.request(req).then([=](web::http::http_response response)-> pplx::task<json::value> {
			// Your code here
			std::wcout << response.status_code() << std::endl;

			if (response.status_code() == status_codes::OK)
			{
				auto headers = response.headers();
				response.headers().set_content_type(L"application/json");
				return response.extract_json();
			}
		}).then([=](pplx::task<json::value> previous_task) mutable {

			// Get the JSON value from the task and call the DisplayJSONValue method
			try
			{
				json::value const & value = previous_task.get();
				DisplayJSONValue(value);
			}
			catch (http_exception const & e)
			{
				std::wcout << e.what() << std::endl;
			}

			if (previous_task._GetImpl()->_HasUserException()) {
				auto holder = previous_task._GetImpl()->_GetExceptionHolder(); // Probably should put in try

				try {
					// Need to make sure you try/catch here, as _RethrowUserException can throw
					holder->_RethrowUserException();
				}
				catch (std::exception& e) {
					// Do what you need to do here
					std::wcout << e.what();
				}
			}
		}).wait();
	}
	catch (std::exception& e)
	{
		std::wcout << e.what();
	}
}

void PostEndFlight()
{
	json::value postData;
	std::string flight_id(flightID);
	utility::string_t flight_id_converted = utility::conversions::to_string_t(flight_id);
	postData[L"flight_plan_id"] = json::value::string(flight_id_converted);
	postData[L"status"] = json::value::number(6);

	http_client_config config;
	config.set_validate_certificates(false);
	http_client client(U("https://utm.avianco.in/v1/flight/updateFlightPlan"), config);

	http_request req(methods::POST);
	req.headers().add(L"Content-Type", L"application/json");

	req.headers().add(L"x-api-key", api_key);
	req.headers().add(L"x-access-token", m_szSessionToken);
	req.headers().set_content_type(U("application/json"));
	req.set_body(postData);
	try
	{

		client.request(req).then([=](web::http::http_response response)-> pplx::task<json::value> {
			// Your code here
			std::wcout << response.status_code() << std::endl;

			if (response.status_code() == status_codes::OK)
			{
				auto headers = response.headers();
				response.headers().set_content_type(L"application/json");
				return response.extract_json();
			}
		}).then([=](pplx::task<json::value> previous_task) mutable {

			// Get the JSON value from the task and call the DisplayJSONValue method
			try
			{
				json::value const & value = previous_task.get();
				DisplayJSONValue(value);
			}
			catch (http_exception const & e)
			{
				std::wcout << e.what() << std::endl;
			}

			if (previous_task._GetImpl()->_HasUserException()) {
				auto holder = previous_task._GetImpl()->_GetExceptionHolder(); // Probably should put in try

				try {
					// Need to make sure you try/catch here, as _RethrowUserException can throw
					holder->_RethrowUserException();
				}
				catch (std::exception& e) {
					// Do what you need to do here
					std::wcout << e.what();
				}
			}
		}).wait();
	}
	catch (std::exception& e)
	{
		std::wcout << e.what();
	}
}

void PostFligtLiveData(int i)
{
	json::value postData;

	postData = CreateFlightPositionJSONObject(i);

	http_client_config config;	
	config.set_validate_certificates(false);
	http_client client(U("https://utm.avianco.in/v1/flight/submitLiveData"), config);

	http_request req(methods::POST);
	req.headers().add(L"Content-Type", L"application/json");

	req.headers().add(L"x-api-key", api_key);
	req.headers().add(L"x-access-token", m_szSessionToken);	
	req.headers().set_content_type(U("application/json"));
	req.set_body(postData);
	try
	{

		client.request(req).then([=](web::http::http_response response)-> pplx::task<json::value> {
			// Your code here
			std::wcout << response.status_code() << std::endl;

			if (response.status_code() == status_codes::OK)
			{
				auto headers = response.headers();
				response.headers().set_content_type(L"application/json");
				return response.extract_json();
			}
		}).then([=](pplx::task<json::value> previous_task) mutable {

			// Get the JSON value from the task and call the DisplayJSONValue method
			try
			{
				json::value const & value = previous_task.get();
				DisplayJSONValue(value);
			}
			catch (http_exception const & e)
			{
				std::wcout << e.what() << std::endl;
			}

			if (previous_task._GetImpl()->_HasUserException()) {
				auto holder = previous_task._GetImpl()->_GetExceptionHolder(); // Probably should put in try

				try {
					// Need to make sure you try/catch here, as _RethrowUserException can throw
					holder->_RethrowUserException();
				}
				catch (std::exception& e) {
					// Do what you need to do here
					std::wcout << e.what();
				}
			}
		}).wait();
	}
	catch (std::exception& e)
	{
		std::wcout << e.what();
	}
}