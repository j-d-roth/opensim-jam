#ifndef OPENSIM_COMAK_TOOL_H_
#define OPENSIM_COMAK_TOOL_H_
/* -------------------------------------------------------------------------- *
 *                                 COMAKTool.h                                *
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

#include "osimPluginDLL.h"
#include <OpenSim\Simulation\Model\Model.h>
#include <OpenSim/Common/FunctionSet.h>
#include <OpenSim/Simulation/Model/ExternalLoads.h>
#include <OpenSim/Simulation/Model/ForceSet.h>
#include <OpenSim/Simulation/Model/AnalysisSet.h>
#include <OpenSim/Simulation/StatesTrajectory.h>

namespace OpenSim { 
class COMAKSecondaryCoordinate;
class COMAKSecondaryCoordinateSet;
class COMAKCostFunctionParameter;
class COMAKCostFunctionParameterSet;

class OSIMPLUGIN_API COMAKTool : public Object{
    OpenSim_DECLARE_CONCRETE_OBJECT(COMAKTool, Object)

public:
    OpenSim_DECLARE_PROPERTY(model_file, std::string, 
        "Path to .osim model to use in COMAK simulation.")

    OpenSim_DECLARE_PROPERTY(coordinates_file, std::string, 
        "Path to input .sto file containing joint angles vs time for all "
        "prescribed, primary, and secondary coordinates.")

    OpenSim_DECLARE_PROPERTY(external_loads_file, std::string, "Path to .xml "
        "file that defines the ExternalLoads applied to the model.")

    OpenSim_DECLARE_PROPERTY(results_directory, std::string, 
        "Path to folder where all results files will be written.")

    OpenSim_DECLARE_PROPERTY(results_prefix, std::string, 
        "Prefix to all results files names.")

    OpenSim_DECLARE_PROPERTY(replace_force_set, bool, 
        "Replace the model ForceSet with the forces listed in force_set_file. "
        "If false, force_set_file forces are appended to the existing model "
        "force set. The default value is false.")

    OpenSim_DECLARE_PROPERTY(force_set_file,std::string,
        "Path to .xml file containing an additional ForceSet.")

    OpenSim_DECLARE_PROPERTY(start_time, double, 
        "First time step of COMAK simulation.")

    OpenSim_DECLARE_PROPERTY(stop_time, double, 
        "Last time step of COMAK simulation.")

    OpenSim_DECLARE_PROPERTY(time_step, double,
        "Time increment between steps in COMAK simulation. Set to -1 to use "
        "the time step in the input coordinates_file. "
        "The default value is -1.")

    OpenSim_DECLARE_PROPERTY(lowpass_filter_frequency, double, 
        "Lowpass filter frequency for input kinematics. "
        "If set to -1, no filtering is applied. The default value is -1.")

    OpenSim_DECLARE_PROPERTY(print_processed_input_kinematics, bool, 
        "Print the processed input Coordinate values, speeds, and "
        "accelerations to a .sto file. These kinematics are used directly "
        "within the COMAK optimization and can be helpful for debugging.")

    OpenSim_DECLARE_LIST_PROPERTY(prescribed_coordinates, std::string,
        "List the paths to the Prescribed Coordinates in the model.")

    OpenSim_DECLARE_LIST_PROPERTY(primary_coordinates, std::string,
        "List the paths to the Primary Coordinates in the model.")

    OpenSim_DECLARE_UNNAMED_PROPERTY(COMAKSecondaryCoordinateSet,
        "List of COMAKSecondaryCoodinate objects.")

    OpenSim_DECLARE_PROPERTY(settle_secondary_coordinates_at_start, bool,
        "Perform a forward simulation to settle secondary coordinates into "
        "equilbrium at initial time step of COMAK. The default value is true.")

    OpenSim_DECLARE_PROPERTY(settle_threshold, double, 
        "Set the maximum change in secondary coordinates between timesteps "
        "that defines equilibrium. Once the change in all "
        "COMAKSecondaryCoordinate values is smaller than the "
        "settle_threshold, the settling simulation is stopped. "
        "The default value is 1e-5.")

    OpenSim_DECLARE_PROPERTY(settle_accuracy, double, 
        "Set the integrator accuracy for initializing forward simulation. "
        "The default value is 1e-6.")

    OpenSim_DECLARE_PROPERTY(print_settle_sim_results, bool, 
        "Print the model states during the forward simulation to a .sto file "
        "in the settle_sim_results_dir.")

    OpenSim_DECLARE_PROPERTY(settle_sim_results_directory, std::string, 
        "Path to the directory where the settling forward simulation results "
        "will be printed.")

    OpenSim_DECLARE_PROPERTY(settle_sim_results_prefix, std::string, 
        "Prefix to settle simulation results file names.")

    OpenSim_DECLARE_PROPERTY(max_iterations, int, 
        "Maximum number of COMAK iterations per time step allowed for the "
        "the simulated model accelerations to converge to the input observed "
        "acceleration values. The default value is 25.")

    OpenSim_DECLARE_PROPERTY(udot_tolerance, double, 
        "Acceptable difference between the simulated accelerations (udots) "
        "and input observed accelerations to determine if the COMAK solution "
        "at each time step has converged. The default value is 1.0.")

    OpenSim_DECLARE_PROPERTY(udot_worse_case_tolerance, double, 
        "Maximum acceptable difference between simulated and observed "
        "accelerations (udots) that is still used if no COMAK iterations "
        "converge. If the max difference between the udots for all iterations "
        "is greater than udot_worse_case_tolerance then no acceptable COMAK "
        "solution was found, and the solution from previous time step is used "
        "for the current time step. The default value is 50.0.")

    OpenSim_DECLARE_PROPERTY(unit_udot_epsilon, double, 
        "The size of the perturbation applied to COMAKSecondaryCoordinates "
        "when computing the gradient of the acceleration constraints in the "
        "COMAK optimization to changes in the secondary coordinate values. "
        "The default value is 1e-8.")

    OpenSim_DECLARE_UNNAMED_PROPERTY(COMAKCostFunctionParameterSet,
        "List of COMAKCostFunctionWeight objects.")

    OpenSim_DECLARE_PROPERTY(contact_energy_weight, double, 
        "The weighting on Smith2018ArticularContactForce potential energy "
        "term in COMAK cost function. The default value is 0.")

    OpenSim_DECLARE_PROPERTY(verbose, int, 
        "Level of debug information reported (0: low, 1: medium, 2: high)")

    OpenSim_DECLARE_PROPERTY(use_visualizer, bool, 
        "Use SimTK visualizer to display simulations in progress. "
        "The default value is false.")

    OpenSim_DECLARE_UNNAMED_PROPERTY(AnalysisSet,"Analyses to be performed"
		"throughout the COMAK simulation.")

//=============================================================================
// METHODS
//=============================================================================
  
    /**
    * Default constructor.
    */
    COMAKTool();
    
    //Construct from .xml file
    COMAKTool(const std::string file);

private:
    void constructProperties();
    void updateModelForces();
    void initialize();
    void extractKinematicsFromFile();
    void applyExternalLoads();
    void printCOMAKascii();
    SimTK::Vector equilibriateSecondaryCoordinates();
    void performCOMAK();
    void setStateFromComakParameters(SimTK::State& state, const SimTK::Vector& parameters);
    SimTK::Vector computeMuscleVolumes();
    void printOptimizationResultsToConsole(const SimTK::Vector& parameters);
    void initializeResultsStorage();
    void recordResultsStorage(const SimTK::State& state, int frame);
    void printResultsFiles();

public:
    void run();
    void setModel(Model& model);


    //--------------------------------------------------------------------------
    // Members
    //--------------------------------------------------------------------------
public:
    Model _model;

    int _n_prescribed_coord;
    int _n_primary_coord;
    int _n_secondary_coord;

    int _n_muscles;
    Array<std::string> _muscle_path;

    int _n_non_muscle_actuators;
    Array<std::string> _non_muscle_actuator_path;

    int _n_actuators;
    int _n_parameters;
    SimTK::Vector _optim_parameters;
    Array<std::string> _optim_parameter_names;

    Array<std::string> _prescribed_coord_name;
    Array<std::string> _prescribed_coord_path;
    Array<int> _prescribed_coord_index;

    Array<std::string> _primary_coord_name;
    Array<std::string> _primary_coord_path;
    Array<int> _primary_coord_index;

    Array<std::string> _secondary_coord_name;
    Array<std::string> _secondary_coord_path;
    Array<int> _secondary_coord_index;

    int _n_frames;
    int _n_out_frames;
    int _start_frame;
    Array<double> _time;
    double _dt;
    int _consecutive_bad_frame;
    std::vector<int> _bad_frames;
    std::vector<double> _bad_times;
    std::vector<double> _bad_udot_errors;
    std::vector<std::string> _bad_udot_coord;

    SimTK::Matrix _q_matrix;
    SimTK::Matrix _u_matrix;
    SimTK::Matrix _udot_matrix;
    ExternalLoads _external_loads;

    SimTK::Vector _secondary_coord_damping;
    SimTK::Vector _secondary_coord_max_change;
    Array<std::string> _secondary_damping_actuator_path;

    SimTK::Vector _optimal_force;
    SimTK::Vector _prev_secondary_value;
    SimTK::Vector _prev_parameters;
    SimTK::Vector _parameter_scale;
    SimTK::Vector _muscle_volumes;
    FunctionSet _cost_muscle_weights;
    std::string _directoryOfSetupFile;

    StatesTrajectory _result_states;
    TimeSeriesTable _result_activations;
    TimeSeriesTable _result_forces;
    TimeSeriesTable _result_kinematics;
    TimeSeriesTable _result_values;
//=============================================================================
};  // END of class COMAK_TOOL

class OSIMPLUGIN_API COMAKSecondaryCoordinate : public Object {
    OpenSim_DECLARE_CONCRETE_OBJECT(COMAKSecondaryCoordinate, Object)

public:
    OpenSim_DECLARE_PROPERTY(coordinate, std::string, 
        "Path to Coordinate in model.")

    OpenSim_DECLARE_PROPERTY(comak_damping, double,
        "Coefficient to penalize frame-to-frame changes in predicted "
        "secondary coordinate values. "
        "The default value is 1.0.")

    OpenSim_DECLARE_PROPERTY(max_change, double, "Limit on the maximum "
        "frame-to-frame changes in secondary coordinate values. "
        "The default value is 0.05.")

    COMAKSecondaryCoordinate();
    void constructProperties();
}; //END of class COMAKSecondaryCoordinate

class OSIMPLUGIN_API COMAKSecondaryCoordinateSet : public Set<COMAKSecondaryCoordinate> {
    OpenSim_DECLARE_CONCRETE_OBJECT(COMAKSecondaryCoordinateSet, Set<COMAKSecondaryCoordinateSet>);

public:
    COMAKSecondaryCoordinateSet();
private:
    void constructProperties();
//=============================================================================
};  // COMAKSecondaryCoordinateSet


class OSIMPLUGIN_API COMAKCostFunctionParameter : public Object {
    OpenSim_DECLARE_CONCRETE_OBJECT(COMAKCostFunctionParameter, Object)

public:
    OpenSim_DECLARE_PROPERTY(actuator, std::string, 
        "Path to actuator in model.")

    OpenSim_DECLARE_PROPERTY(weight, Function, 
        "Weighting coefficient that multiplies the squared actuator "
        "activation in the COMAK optimization cost function."
        "The Default value is 1.0.")


    COMAKCostFunctionParameter();
    void constructProperties();
}; //END of class COMAKCostFunctionParameter

class OSIMPLUGIN_API COMAKCostFunctionParameterSet : public Set<COMAKCostFunctionParameter> {
    OpenSim_DECLARE_CONCRETE_OBJECT(COMAKCostFunctionParameterSet, Set<COMAKCostFunctionParameter>)

public:

    COMAKCostFunctionParameterSet();
    void constructProperties();
}; //END of class COMAKCostFunctionParameterSet
}; //namespace
//=============================================================================
//=============================================================================

#endif // OPENSIM_COMAK_TOOL_H_


