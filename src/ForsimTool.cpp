/* -------------------------------------------------------------------------- *
 *                                ForsimTool.cpp                              *
 * -------------------------------------------------------------------------- *
 * Author(s): Colin Smith                                                     *
 *                                                                            *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may    *
 * not use this file except in compliance with the License. You may obtain a  *
 * copy of the License at http://www.apache.org/licenses/LICENSE-2.0.         *
 *                                                                            *
 * Unless required by applicable law or agreed to in writing, software        *
 * distributed under the License is distributed on an "AS IS" BASIS,          *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   *
 * See the License for the specific language governing permissions and        *
 * limitations under the License.                                             *
 * -------------------------------------------------------------------------- */


//=============================================================================
// INCLUDES
//=============================================================================
#include <OpenSim/Simulation/Model/Model.h>
#include "ForsimTool.h"
#include "HelperFunctions.h"
#include <OpenSim/Common/STOFileAdapter.h>
#include <OpenSim/OpenSim.h>
#include <OpenSim/Common/Constant.h>
#include <OpenSim/Common/FunctionSet.h>
#include <OpenSim\Actuators\Millard2012EquilibriumMuscle.h>
#include <Blankevoort1991Ligament.h>

using namespace OpenSim;


//=============================================================================
// CONSTRUCTOR(S)
//=============================================================================

ForsimTool::ForsimTool() : Object()
{
	setNull();
	constructProperties();

}

ForsimTool::ForsimTool(std::string settings_file) : Object(settings_file) {
	constructProperties();
	updateFromXMLDocument();
	loadModel(settings_file);
	
    _directoryOfSetupFile = IO::getParentDirectory(settings_file);	
    IO::chDir(_directoryOfSetupFile); 
}


//_____________________________________________________________________________
/**
 * Set all member variables to their null or default values.
 */
void ForsimTool::setNull()
{
    setAuthors("Colin Smith");

}
//_____________________________________________________________________________
/**
 * Connect properties to local pointers.
 */
void ForsimTool::constructProperties()
{
	constructProperty_model_file("");
	constructProperty_actuator_input_file("");
	constructProperty_external_loads_file("");
	constructProperty_prescribed_coordinates_file("");	
	constructProperty_results_directory(".");
	constructProperty_results_file_basename("");
	constructProperty_start_time(-1);
	constructProperty_stop_time(-1);
	constructProperty_integrator_accuracy(0.000001);
	constructProperty_report_time_step(0.01);
	constructProperty_minimum_time_step(0.00000001);
	constructProperty_maximum_time_step(0.01);
	constructProperty_constant_muscle_frc(-1);
	constructProperty_unconstrained_coordinates();
	constructProperty_use_visualizer(false);
    constructProperty_verbose(0);
	constructProperty_AnalysisSet(AnalysisSet());
}


void ForsimTool::setModel(Model& aModel)
{
	_model = aModel;
	set_model_file(_model.getDocumentFileName());
}

void ForsimTool::run() 
{	
	
	SimTK::State state = _model.initSystem();
	
	if (get_use_visualizer()) {
		_model.setUseVisualizer(true);
	}	
	
	//Add Analysis set
    AnalysisSet aSet = get_AnalysisSet();
	int size = aSet.getSize();

    for(int i=0;i<size;i++) {
        Analysis *analysis = aSet.get(i).clone();
        _model.addAnalysis(analysis);
    }
	
	//Apply External Loads
	applyExternalLoads();
	
	//Prescribe Coordinates in the Model
	initializeCoordinates();
	state = _model.initSystem();
	
	//Apply Muscle Forces
	initializeActuators(state);
		
	//Set Start and Stop Times
    initializeStartStopTimes();

	//Allocate Results Storage
    StatesTrajectory result_states;

	_model.equilibrateMuscles(state);

	//Setup Visualizer
	if (get_use_visualizer()) {
		_model.updMatterSubsystem().setShowDefaultGeometry(false);
		SimTK::Visualizer& viz = _model.updVisualizer().updSimbodyVisualizer();
		viz.setBackgroundColor(SimTK::Black);
        viz.setBackgroundType(SimTK::Visualizer::BackgroundType::SolidColor);
        viz.setMode(SimTK::Visualizer::Mode::Sampling);
		viz.setShowSimTime(true);
        viz.setDesiredFrameRate(100);
	}

	//Set Integrator
    state.setTime(get_start_time());
    
	SimTK::CPodesIntegrator integrator(_model.getSystem(), SimTK::CPodes::BDF, SimTK::CPodes::Newton);
	integrator.setAccuracy(get_integrator_accuracy());
	integrator.setMinimumStepSize(get_minimum_time_step());
	integrator.setMaximumStepSize(get_maximum_time_step());
    
	SimTK::TimeStepper timestepper(_model.getSystem(), integrator);
	timestepper.initialize(state);
    
	//Integrate Forward in Time
	double dt = get_report_time_step();
	int nSteps = round((get_stop_time() - get_start_time()) / dt);
	AnalysisSet& analysisSet = _model.updAnalysisSet();

	std::cout << std::endl;
	std::cout << std::endl;
	std::cout << "Performing Forward Dynamic Simulation" << std::endl;
	std::cout << "Start Time: " << get_start_time() << std::endl;
	std::cout << "Stop Time: " << get_stop_time() << std::endl;
	std::cout << std::endl;

	for (int i = 0; i <= nSteps; ++i) {
        
		double t = get_start_time() + i * dt;
		std::cout << "Time:" << t << std::endl;

        if (get_verbose() >= 2) {
            printDebugInfo(state);
        }

		//Set Prescribed Muscle Forces
		for (int j = 0; j < _prescribed_frc_actuator_paths.size();++j) {           
			std::string actuator_path = _prescribed_frc_actuator_paths[j];
			ScalarActuator& actuator = _model.updComponent<ScalarActuator>(actuator_path);
			double value = _frc_functions.get(actuator_path +"_frc").calcValue(SimTK::Vector(1,t));
			actuator.setOverrideActuation(state, value);
		}

        timestepper.initialize(state);
		timestepper.stepTo(t);

        state = timestepper.updIntegrator().updAdvancedState();

		//Record parameters
		if (i == 0) {
			analysisSet.begin(state);
		}
		else {
			analysisSet.step(state, i);
		}

        result_states.append(state);        
	}

	//Print Results
    TimeSeriesTable states_table = result_states.exportToTable(_model);
    states_table.addTableMetaData("header", std::string("CoordinateValues"));
	states_table.addTableMetaData("nRows", std::to_string(states_table.getNumRows()));
	states_table.addTableMetaData("nColumns", std::to_string(states_table.getNumColumns()+1));
	states_table.addTableMetaData("inDegrees", std::string("yes"));

	STOFileAdapter sto;
	std::string basefile = get_results_directory() + "/" + get_results_file_basename();

    sto.write(states_table, basefile + "_states.sto");
	
	_model.updAnalysisSet().printResults(get_results_file_basename(), get_results_directory());

	std::cout << "\nSimulation complete." << std::endl;
	std::cout << "Printed results to: " + get_results_directory() << std::endl;
}

void ForsimTool::initializeStartStopTimes() {
    if (get_start_time() != -1 && get_stop_time() != -1) {
		return;
	}

    double start = 0;
    double end = 0;
    double act_start = -2;
    double act_end = -2;
    double coord_start = -2;
    double coord_end = -2;

    if (get_actuator_input_file() != "") {
        std::vector<double> act_time = _actuator_table.getIndependentColumn();
        act_start = act_time[0];
        act_end = act_time.back();
    }
    if (get_prescribed_coordinates_file() != "") {
        std::vector<double> coord_time = _coord_table.getIndependentColumn();
        coord_start = coord_time[0];
        coord_end = coord_time.back();
    }

    if (act_start == -2 && coord_start == -2) {
        OPENSIM_THROW(Exception, "No actuator_input_file or "
        "prescribed_coordinates_files defined." 
        "You must set start_time and stop_time to non-negative values.")
    }

    if (act_start != -2 && coord_start != -2) {
        if (act_start != coord_start || act_end != coord_end) {
            OPENSIM_THROW(Exception, "The start and stop times of the "
                "actuator_input_file and prescribed coordinate files do not match."
                "You must set start_time and stop_time to non-negative values.")
        }
    }

    if (get_start_time() == -1) {
        if (coord_start != -2)
            set_start_time(coord_start);
        else
            set_start_time(act_start);;
	}
	if (get_stop_time() == -1) {
		if (coord_end != -2)
            set_stop_time(coord_end);
        else
            set_stop_time(act_end);;
	}
}

void ForsimTool::initializeActuators(SimTK::State& state) {
    PrescribedController* control = new PrescribedController();
	if (!(get_actuator_input_file() == "")){
		STOFileAdapter actuator_file;
		_actuator_table = actuator_file.read(get_actuator_input_file());

		int nDataPt = _actuator_table.getNumRows();
		std::vector<std::string> labels = _actuator_table.getColumnLabels();
		std::vector<double> time = _actuator_table.getIndependentColumn();
        
		//Set minimum activation
		for (int i = 0; i < labels.size(); ++i) {
			std::vector<std::string> split_label = split_string(labels[i], "_");

			if (split_label.back() == "frc") {
				std::string actuator_path = erase_sub_string(labels[i], "_frc");                
				try {
					ScalarActuator& actuator = _model.updComponent<ScalarActuator>(actuator_path);
					actuator.overrideActuation(state, true);
					_prescribed_frc_actuator_paths.push_back(actuator_path);
					SimTK::Vector values = _actuator_table.getDependentColumn(labels[i]);
					SimmSpline* frc_function = new SimmSpline(nDataPt, &time[0], &values[0], actuator_path + "_frc");

					_frc_functions.adoptAndAppend(frc_function); 
				}
				catch (ComponentNotFoundOnSpecifiedPath) {
					OPENSIM_THROW(Exception,
						"Actuator: " + actuator_path + " not found in model. "
						"Did you use absolute path?")
				}
			}

			if (split_label.back() == "act") {
				std::string actuator_path = erase_sub_string(labels[i], "_act");
				try {
					Millard2012EquilibriumMuscle& msl = _model.updComponent<Millard2012EquilibriumMuscle>(actuator_path);

                    _prescribed_act_actuator_paths.push_back(actuator_path);

					SimTK::Vector values = _actuator_table.getDependentColumn(labels[i]);
					SimmSpline* act_function = new SimmSpline(nDataPt, &time[0], &values[0], actuator_path + "_act");

					control->addActuator(msl);

					control->prescribeControlForActuator(msl.getName(), act_function);

					msl.set_ignore_activation_dynamics(true);
				}
				catch (ComponentNotFoundOnSpecifiedPath) {
					OPENSIM_THROW(Exception,
						"Muscle: " + actuator_path + " not found in model. "
						"Did you use absolute path? Is it a Millard2012EquilibriumMuscle?")
				}
			}

			if (split_label.back() == "control") {
				std::string actuator_path = erase_sub_string(labels[i], "_control");
				try {
					ScalarActuator& actuator = _model.updComponent<ScalarActuator>(actuator_path);
					_prescribed_control_actuator_paths.push_back(actuator_path);
					SimTK::Vector values = _actuator_table.getDependentColumn(labels[i]);

					SimmSpline* control_function = new SimmSpline(nDataPt, &time[0], &values[0], actuator_path + "_control");

					control->addActuator(actuator);

					control->prescribeControlForActuator(actuator.getName(), control_function);

					std::cout << "Control Prescribed: " << actuator_path << std::endl;
				}
				catch (ComponentNotFoundOnSpecifiedPath) {
					OPENSIM_THROW(Exception,
						"Actuator: " + actuator_path + " not found in model. "
						"Did you use absolute path?")
				}

			}
		}

		//Output to Screen
		if (_prescribed_frc_actuator_paths.size() > 0) {
			std::cout << std::endl;
			std::cout << "Force Prescribed:" << std::endl;
			for (std::string& name : _prescribed_frc_actuator_paths) {
				std::cout << name << std::endl;
			}
			std::cout << std::endl;
		}

		if (_prescribed_act_actuator_paths.size() > 0) {
			std::cout << "Activation Prescribed:" << std::endl;
			for (std::string& name : _prescribed_act_actuator_paths) {
				std::cout << name << std::endl;
			}
			std::cout << std::endl;
		}

		if (_prescribed_control_actuator_paths.size() > 0) {
			std::cout << "Control Prescribed:" << std::endl;
			for (std::string& name : _prescribed_control_actuator_paths) {
				std::cout << name << std::endl;
			}
			std::cout << std::endl;
		}        
	}
    _model.addComponent(control);
    state = _model.initSystem();

	//Set Constant Muscle Activation
	if (get_constant_muscle_frc() > -1) {
		std::cout << "Constant Muscle Force Multiplier: " << get_constant_muscle_frc() << std::endl;
		for (Muscle& msl : _model.updComponentList<Muscle>()) {
			std::string msl_path = msl.getAbsolutePathString();

			if (contains_string(_prescribed_frc_actuator_paths, msl_path)) {
				continue;
			}
			if (contains_string(_prescribed_act_actuator_paths, msl_path)) {
				continue;
			}
			if (contains_string(_prescribed_control_actuator_paths, msl_path)) {
				continue;
			}		

			_prescribed_frc_actuator_paths.push_back(msl_path);
			
            double const_frc = get_constant_muscle_frc() * msl.getMaxIsometricForce();
			Constant* frc_function = new Constant();
			frc_function->setName(msl_path + "_frc");

			_frc_functions.adoptAndAppend(frc_function);


			std::cout << msl_path << std::endl;
		}
		std::cout << std::endl;
	}

    for (int j = 0; j < _prescribed_frc_actuator_paths.size();++j) {           
		std::string actuator_path = _prescribed_frc_actuator_paths[j];
		ScalarActuator& actuator = _model.updComponent<ScalarActuator>(actuator_path);
        actuator.overrideActuation(state, true);
	}    
}

void ForsimTool::initializeCoordinates() {	
	for (Coordinate& coord : _model.updComponentList<Coordinate>()) {
		coord.set_locked(true);
	}

	std::cout << "\nUnconstrained Coordinates:" << std::endl;
	for (int i = 0; i < getProperty_unconstrained_coordinates().size(); ++i) {
		std::string coord_path = get_unconstrained_coordinates(i);

		try {
			Coordinate& coord = _model.updComponent<Coordinate>(coord_path);
			coord.set_locked(false);
			std::cout <<  coord_path << std::endl;
		}
		catch(ComponentNotFoundOnSpecifiedPath){OPENSIM_THROW(Exception,
			"Unconstrained Coordinate: " +  coord_path + "Not found in model."
			"Did you use absolute path?") }
	}	
    
	//Load prescribed coordinates file
	STOFileAdapter coord_file;
    if (get_prescribed_coordinates_file() != "") {

        std::string saveWorkingDirectory = IO::getCwd();
        IO::chDir(_directoryOfSetupFile);

        try {
             _coord_table = coord_file.read(get_prescribed_coordinates_file());
      
        } catch(...) { // Properly restore current directory if an exception is thrown
            IO::chDir(saveWorkingDirectory);
            throw;
        }
        IO::chDir(saveWorkingDirectory);
        
        std::vector<std::string> labels = _coord_table.getColumnLabels();

        int nDataPt = _coord_table.getNumRows();
        std::vector<double> time = _coord_table.getIndependentColumn();

        std::cout << "\nPrescribed Coordinates:" << std::endl;
        for (int i = 0; i < labels.size(); ++i) {
            try {
                Coordinate& coord = _model.updComponent<Coordinate>(labels[i]);
                SimTK::Vector values = _coord_table.getDependentColumn(labels[i]);
                if (coord.getMotionType() == Coordinate::MotionType::Rotational) {
                    values *= SimTK::Pi / 180;
                }

                SimmSpline function = SimmSpline(nDataPt, &time[0], &values[0], coord.getName() + "_prescribed");
                coord.set_prescribed(true);
                coord.set_prescribed_function(function);
                coord.set_locked(false);

                std::cout << labels[i] << std::endl;
            }
            catch (ComponentNotFoundOnSpecifiedPath) { OPENSIM_THROW(Exception, "Prescribed Coordinate: " + labels[i] + "was not found in model. Did you use absolute path?") }
        }
        std::cout << std::endl;
    }
}

void ForsimTool::applyExternalLoads()
{
	const std::string& aExternalLoadsFileName = get_external_loads_file();

	if (aExternalLoadsFileName == "" || aExternalLoadsFileName == "Unassigned") {
		std::cout << "No external loads will be applied (external loads file not specified)." << std::endl;
		return;
	}

	// This is required so that the references to other files inside ExternalLoads file are interpreted 
    // as relative paths
    std::string savedCwd = IO::getCwd();
    IO::chDir(IO::getParentDirectory(aExternalLoadsFileName));
    // Create external forces
    ExternalLoads* externalLoads = nullptr;
    try {
        externalLoads = new ExternalLoads(aExternalLoadsFileName, true);
        _model.addModelComponent(externalLoads);
    }
    catch (const Exception &ex) {
        // Important to catch exceptions here so we can restore current working directory...
        // And then we can re-throw the exception
        std::cout << "Error: failed to construct ExternalLoads from file " << aExternalLoadsFileName;
        std::cout << ". Please make sure the file exists and that it contains an ExternalLoads";
        std::cout << "object or create a fresh one." << std::endl;
        if (getDocument()) IO::chDir(savedCwd);
        throw(ex);
    }

    // copy over created external loads to the external loads owned by the tool
    _external_loads = *externalLoads;

    IO::chDir(savedCwd);
    return;
}

void ForsimTool::loadModel(const std::string &aToolSetupFileName)
{
    
	OPENSIM_THROW_IF(get_model_file().empty(), Exception,
            "No model file was specified (<model_file> element is empty) in "
            "the Setup file. ");
    std::string saveWorkingDirectory = IO::getCwd();
    std::string directoryOfSetupFile = IO::getParentDirectory(aToolSetupFileName);
    IO::chDir(directoryOfSetupFile);

    std::cout<<"ForsimTool "<< getName() <<" loading model '"<<get_model_file() <<"'"<< std::endl;

	Model model;

    try {
        model = Model(get_model_file());
		model.finalizeFromProperties();
        
    } catch(...) { // Properly restore current directory if an exception is thrown
        IO::chDir(saveWorkingDirectory);
        throw;
    }
    _model = model;
    IO::chDir(saveWorkingDirectory);
}

void ForsimTool::printDebugInfo(const SimTK::State& state) {
    _model.realizeReport(state);
    int w = 20;

    std::cout << std::setw(w) << "Muscle" 
        << std::setw(w) << "Force"  
        << std::setw(w) << "Activation"
        << std::setw(w) << "Control" << std::endl;

    for (const Muscle& msl : _model.updComponentList<Muscle>()) {
        std::cout << std::setw(w) << msl.getName()
            << std::setw(w) << msl.getActuation(state)
            << std::setw(w) << msl.getActivation(state)
            << std::setw(w) << msl.getControl(state) 
            << std::endl;
    }
    std::cout << std::endl;

    std::cout << std::setw(w) << "Ligament " 
        << std::setw(w) << "Total Force" 
        << std::setw(w) << "Spring Force" 
        << std::setw(w) << "Damping Force" 
        << std::setw(w) << "Strain" 
        << std::setw(w) << "Strain Rate" 
        << std::setw(w) << "Length" 
        << std::setw(w) << "Lengthening Rate" 
        << std::endl;

            

    for (const Blankevoort1991Ligament& lig : _model.updComponentList<Blankevoort1991Ligament>()) {
        std::cout << std::setw(w) << lig.getName()
            << std::setw(w) << lig.getOutputValue<double>(state, "force_total")
            << std::setw(w) << lig.getOutputValue<double>(state, "force_spring")
            << std::setw(w) << lig.getOutputValue<double>(state, "force_damping")
            << std::setw(w) << lig.getOutputValue<double>(state, "strain")
            << std::setw(w) << lig.getOutputValue<double>(state, "strain_rate")
            << std::setw(w) << lig.getOutputValue<double>(state, "length")
            << std::setw(w) << lig.getOutputValue<double>(state, "lengthening_rate")                    
            << std::endl;
    }
    std::cout << std::endl;

    std::cout << std::setw(w) << "Contact " << std::setw(20) 
        << "Force" << std::setw(w) << "COP" << std::endl;

    for (Smith2018ArticularContactForce& cnt : _model.updComponentList<Smith2018ArticularContactForce>()) {
        cnt.getOutputValue<SimTK::Vec3>(state, "casting_total_contact_force");
        cnt.getOutputValue<SimTK::Vec3>(state, "casting_total_center_of_pressure");


        std::cout << std::setw(w) <<cnt.getName() 
            << std::setw(w) << cnt.getOutputValue<SimTK::Vec3>(state, "casting_total_contact_force") 
            << std::setw(w) << cnt.getOutputValue<SimTK::Vec3>(state, "casting_total_center_of_pressure")
            << std::endl;
    }
    std::cout << std::endl;    
    std::cin.ignore();
    std::cout << "Press Any Key to Continue." << std::endl;
     
}