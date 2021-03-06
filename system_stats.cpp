#include <iostream>
#include <stdio.h>
#include <string>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <vector>
#include <syslog.h>

#include "system_stats.h"
#include "unix_functions.h"
#include "config.h"

using namespace std;



// helper function to get a string from the status type
string getSectionFromType(status type) {
  string res;
  switch (type) {
    case CPU: res     = "cpu";     break;
    case Load: res    = "load";    break;
    case HDD: res     = "hdd";     break;
    case Mount: res   = "mount";   break;
    case Memory: res  = "memory";  break;
    case Network: res = "network"; break;
    default: throw "No status type submitted."; break;
  }
  
  return res;
}


/*****************************************************************
**
**  PUBLIC STUFF
**
*****************************************************************/

SystemStats::SystemStats(status type, string configFile){
  // set the type (cpu, load, hdd, mount or memory):
  _type = type;
  
  // now init the generic class with the config file
  if (loadConfigFile(configFile)) {
    if (_delta) { _array_size++; }
    initArray();
  } 
}

SystemStats::~SystemStats() {
  delete [] _list;
}




// function to save the systems temperature into array
void SystemStats::readStatus() {
  // read requested values:
  std::vector<double> newVal;
  for (int i = 0; i < _element_count; i++) {
    string cmd = _cmd[i];
    const char* cmd_output = &getCmdOutput(&cmd[0])[0];
    newVal.push_back(atof(cmd_output));
  }
  
  // save values into array
  setValue(getReadableTime(), newVal);

  // write json file:
  writeJSONFile();
}


// this will load the contents from a json file into the array:
bool SystemStats::loadFromFile(){
  std::vector<string> lines;
  string pattern = "{ \"title\" : ";
  
  // find lines that have datapoints:
  ifstream file(_filepath + _section + ".json");
  string str; 
  while (getline(file, str))
  {
    if (str.find(pattern) != std::string::npos) {
      lines.push_back(str);
    }
  }
  
  // now all data values are inside the lines-vector.
  // check if their size matches the configuration:
  int datasize = _array_size * _element_count; 
  if (_delta) { datasize -= _element_count; }
  if (lines.size() != datasize) {
    syslog(LOG_WARNING, "%s: Could not load existing json files. Datapoints mismatch configuration file.", _section.c_str());
    return false;
  }
  
  // extract data and add them to the array
  string tmp;
  string tim;
  string va;
  string pattern2 = "\"value\" : ";
  std::vector<double> val;
  for (int j = 0; j < datasize/_element_count; j++) {
    // 1) get time:
    tmp = lines[j];
    tmp.erase(0, tmp.find(pattern) + pattern.size());
    tim = tmp.substr(tmp.find("\"")+1, tmp.find("\"", 2)-1);
    
    // 2) get values:
    val.clear();
    for (int i = 0; i < _element_count; i++) {
      tmp = lines[j + (i*_array_size)];
      tmp.erase(0, tmp.find(pattern2) + pattern2.size());  
      va = tmp.substr(0, tmp.find("},"));
      // if atof fails it will write "0"
      val.push_back(atof(va.c_str()));
    }
    
    // 3) add record
    setValue(tim, val);
  }

  return true;
}



/*****************************************************************
**
**  PRIVATE STUFF
**
*****************************************************************/

// loads the ini-based config and sets the variables
bool SystemStats::loadConfigFile(string configFile) {
	
  if (FileExists(configFile)) {
	  _section = getSectionFromType(_type);
    config *configuration = new config(configFile);

    // read array sizes:
    _array_size = configuration->readElementCount(_section);

    // read number of informations:
    _element_count = configuration->readSequenceCount(_section);
    for (int i=1; i<=_element_count; i++) {
      _cmd.push_back(configuration->readSequenceCommand(_section, i-1));
      _description.push_back(configuration->readSequenceTitle(_section, i-1));
      _color.push_back(configuration->readSequenceColor(_section, i-1));
    }

    if (configuration->readJSONType(_section) == "bar") {
      _json_type = bar;
    } else {
      _json_type = line;
    }
    
    _delta = configuration->readDelta(_section);
    
    _refresh_interval = configuration->readJSONRefreshInterval(_section);

    _interval = configuration->readInterval(_section);
    _filepath = configuration->readFilepath();
    
    syslog(LOG_DEBUG, "All configuration loaded");
    
    return true;
  }

  return false;
}




// creates an array and fills it with empty values
void SystemStats::initArray() {

  // alloc memory for array
  _list = new data[_array_size];
  
  // init array with empty values
  for (int i = 0; i < _array_size; i++) {
    _list[i].timestamp    = "0";

    for (int j = 0; j < _element_count; j++) {
      _list[i].value.push_back(0);
    }
  }

  // set pointer
  _list_position = 0;
}




// Array setter:
void SystemStats::setValue(std::string time, std::vector<double> value) {
	
  if (value.size() == _element_count) {
    
    // save timestamp:
    _list[_list_position].timestamp = time;

    // get values:
    _list[_list_position].value.clear();
    for (int i=0; i<_element_count; i++) {
      _list[_list_position].value.push_back(value[i]);
    }

    // move pointer:
    if (_list_position < _array_size - 1) {
      _list_position++;
    } else {
      _list_position = 0;
    }

  }
}


void SystemStats::Inc(int &value) {
  if (value < _array_size) { value++; }
  else { value = 0; }
}



// generic json export function
void SystemStats::writeJSONFile() {
  ofstream _out_file;
  _out_file.open(_filepath + _section + ".json");
  _out_file << "{ \n";
  _out_file << "  \"graph\" : { \n";
  _out_file << "  \"title\" : \"" + _section + "\", \n";
  
  string _graphtype;
  switch (_json_type) {
    case bar: _graphtype = "bar"; break;
    default: _graphtype = "line"; break;
  }  
  _out_file << "  \"type\" : \"" + _graphtype + "\", \n";
  _out_file << "  \"refreshEveryNSeconds\" : " << _refresh_interval << ", \n";
  
  _out_file << "  \"datasequences\" : [ \n";
  
  // for every load value print a line:
  for (int j = 0; j < _element_count; j++) {
	
    _out_file << "      {";
    _out_file << "      \"title\": \"" << _description[j] << + "\", \n";
    if (_color[j] != "-") {
      _out_file << "             \"color\": \"" << _color[j] << + "\", \n";
    }
    _out_file << "             \"datapoints\" : [ \n";


    double _val;
    int _start_position = _list_position;
    if (_delta) { Inc(_start_position); }
    for (int i = _start_position; i < _array_size; i++) {
      string val;
      stringstream out;
      _val = _list[i].value[j];
      if (_delta) { if (i == 0) { _val -= _list[_array_size-1].value[j]; } else { _val -= _list[i-1].value[j]; } }
      out << _val;
      _out_file << "               { \"title\" : \"" + _list[i].timestamp + "\", \"value\" : " + out.str() + "}, \n";
    }

    if (_list_position != 0) {
      // print the remaining elements
      for (int i=0; i < _list_position; i++) {
        string val;
        stringstream out;
        _val = _list[i].value[j];
        if (_delta) { if (i == 0) { _val -= _list[_array_size-1].value[j]; } else { _val -= _list[i-1].value[j]; } }
        out << _val;
        _out_file << "               { \"title\" : \"" + _list[i].timestamp + "\", \"value\" : " + out.str() + "}, \n";
      }
    }

    _out_file << "             ] \n";
    if (j == _element_count - 1) {
	  _out_file << "      } \n";
    } else {
      _out_file << "      }, \n";
    }

  }
  
  _out_file << "  ] \n";
  _out_file << "  } \n";
  _out_file << "} \n";
  
  _out_file.close();
}