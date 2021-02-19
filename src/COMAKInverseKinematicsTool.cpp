/* -------------------------------------------------------------------------- *
 *                     COMAKInverseKinematicsTool.cpp                         *
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
#include <OpenSim/Common/XMLDocument.h>
#include <OpenSim.h>
#include "COMAKInverseKinematicsTool.h"
#include "HelperFunctions.h"
#include <OpenSim/Common/IO.h>

using namespace OpenSim;
using namespace SimTK;

//=============================================================================
// CONSTRUCTOR(S) AND DESTRUCTOR
//=============================================================================

//_____________________________________________________________________________
/**
 * Default constructor.
 */
COMAKInverseKinematicsTool::COMAKInverseKinematicsTool() 
{
    constructProperties();
}

COMAKInverseKinematicsTool::COMAKInverseKinematicsTool(const std::string file) : Object(file) {
    constructProperties();
    updateFromXMLDocument();
        
    _directoryOfSetupFile = IO::getParentDirectory(file);	
    IO::chDir(_directoryOfSetupFile); 
}

//_____________________________________________________________________________
/**
 * Connect properties to local pointers.
 */
void COMAKInverseKinematicsTool::constructProperties()
{
    constructProperty_model_file("");
    constructProperty_results_directory("");
    constructProperty_results_prefix("");

    constructProperty_perform_secondary_constraint_sim(true);
    constructProperty_secondary_coordinates();
    constructProperty_secondary_coupled_coordinate("");
    constructProperty_secondary_constraint_sim_settle_threshold(1e-5);
    constructProperty_secondary_constraint_sim_sweep_time(1.0);
    constructProperty_secondary_coupled_coordinate_start_value(0.0);
    constructProperty_secondary_coupled_coordinate_stop_value(0.0);
    constructProperty_secondary_constraint_sim_integrator_accuracy(1e-6);
    constructProperty_secondary_constraint_sim_internal_step_limit(-1);
    constructProperty_constraint_function_num_interpolation_points(20);
    constructProperty_secondary_constraint_function_file(
        "secondary_coordinate_constraint_functions.xml");
    constructProperty_print_secondary_constraint_sim_results(false);

    constructProperty_perform_inverse_kinematics(true);
    constructProperty_IKTaskSet(IKTaskSet());
    constructProperty_marker_file("");
    constructProperty_coordinate_file("");
    constructProperty_output_motion_file("");
    constructProperty_ik_constraint_weight(Infinity);
    constructProperty_ik_accuracy(1e-5);
    Array<double> range{SimTK::Infinity, 2};
    range[0] = -SimTK::Infinity; 
    constructProperty_time_range(range);
    constructProperty_report_errors(false);
    constructProperty_report_marker_locations(false);
    constructProperty_constrained_model_file("");
    constructProperty_use_visualizer(false);
    constructProperty_verbose(0);
}

void COMAKInverseKinematicsTool::initialize()
{
    //Make results directory
    int makeDir_out = IO::makeDir(get_results_directory());
    if (errno == ENOENT && makeDir_out == -1) {
        OPENSIM_THROW(Exception, "Could not create " +
            get_results_directory() +
            "Possible reason: This tool cannot make new folder with subfolder.");
    }

    _model = Model(get_model_file());

    std::string function_file = get_secondary_constraint_function_file();

    if (get_secondary_constraint_function_file() == "") {
        OPENSIM_THROW(Exception, "secondary_constraint_function file not set.")
    }
    
    _model.initSystem();

    //Verfiy Coordinate Properties    
    for (Coordinate& coord : _model.updComponentList<Coordinate>()) {
        std::string name = coord.getName();
        std::string path = coord.getAbsolutePathString();

        //Reset to full path
        if (get_secondary_coupled_coordinate() == name) {
            set_secondary_coupled_coordinate(path);
        }

        int ind = getProperty_secondary_coordinates().findIndex(name);

        if (ind > -1) {
            set_secondary_coordinates(ind, path);
        }
    }

    //Make sure Coordinate exists in model and no duplicates	
    std::string name = get_secondary_coupled_coordinate();
    try { _model.getComponent<Coordinate>(name); }
    catch (Exception) {
        OPENSIM_THROW(Exception, "secondary_coupled_coord: " + 
            name + " not found in model.")
    }
    
    for (int i = 0; i < getProperty_secondary_coordinates().size(); ++i) {
        std::string name = get_secondary_coordinates(i);
        try { _model.getComponent<Coordinate>(name); }
        catch (Exception){
            OPENSIM_THROW(Exception,"secondary_coordinate: " + 
                name + "not found in model.")
        }

        int n = 0;
        for (int j = 0; j < getProperty_secondary_coordinates().size(); ++j) {
            if (name == get_secondary_coordinates(j)) n++;
        }
        OPENSIM_THROW_IF(n>1, Exception, name + 
            "listed multiple times in secondary_coordinates")
    }

    //Count numbers
    _n_secondary_coord = getProperty_secondary_coordinates().size();

    _secondary_coord_name.setSize(_n_secondary_coord);
    _secondary_coord_path.setSize(_n_secondary_coord);
    _secondary_coord_index.setSize(_n_secondary_coord);


    for (int i = 0; i < _n_secondary_coord; ++i) {
        _secondary_coord_path[i] = get_secondary_coordinates(i);
        _secondary_coord_name[i] = _model.getComponent<Coordinate>
            (get_secondary_coordinates(i)).getName();
    }

    int nCoord = 0;
    for (Coordinate& coord : _model.updComponentList<Coordinate>()) {
        std::string path = coord.getAbsolutePathString();

        int ind = _secondary_coord_path.findIndex(path);
        if (ind > -1) {
            _secondary_coord_index[ind] = nCoord;
        }
        nCoord++;
    }

    std::cout << std::endl;
    std::cout << "Secondary Coordinates:" << std::endl;
    std::cout << "----------------------" << std::endl;
    for (int i = 0; i < _n_secondary_coord; ++i) {
        std::cout << _secondary_coord_name[i] << std::endl;
    }
    std::cout << std::endl;
    std::cout << "Secondary Coupled Coordinate: " <<
        get_secondary_coupled_coordinate() << std::endl;

    if (get_perform_secondary_constraint_sim()) {
        std::cout << "Settle Threshold: " <<
            get_secondary_constraint_sim_settle_threshold() << std::endl;

        std::cout << "Sweep Time: " <<
            get_secondary_constraint_sim_sweep_time() << std::endl;

        std::cout << "Sweep secondary_coupled_coordinate start value: " <<
            get_secondary_coupled_coordinate_start_value() << std::endl;

        std::cout << "Sweep secondary_coupled_coordinate stop value: " <<
            get_secondary_coupled_coordinate_stop_value() << std::endl;

        std::cout << std::endl;
    }

    _state = _model.initSystem();
}

void COMAKInverseKinematicsTool::run()
{
    //Secondary Constraint Simulation
    if (get_perform_secondary_constraint_sim()) {
        performIKSecondaryConstraintSimulation();
    }

    //Inverse Kinematics 
    if (get_perform_inverse_kinematics()) {
        performIK();
    }
}


void COMAKInverseKinematicsTool::performIKSecondaryConstraintSimulation() {
    std::cout << "Performing IK Secondary Constraint Simulation..." << std::endl;
    
    //Initialize Model
    Model model = _model;
    model.setUseVisualizer(get_use_visualizer());
    model.initSystem();

    for (Muscle& msl : model.updComponentList<Muscle>()) {
        if (msl.getConcreteClassName() == "Millard2012EquilibriumMuscle") {
            msl.set_ignore_activation_dynamics(true);
            msl.set_ignore_tendon_compliance(true);
        }
    }
        
    for(auto& cc_const : model.updComponentList<CoordinateCouplerConstraint>()){
        std::string cc_coord_name = cc_const.getDependentCoordinateName();
        Coordinate& coord = model.updCoordinateSet().get(cc_coord_name);
        coord.set_locked(false);
    }

    //Set coordinate types
    for (auto& coord : model.updComponentList<Coordinate>()) {
        if (getProperty_secondary_coordinates().findIndex(coord.getAbsolutePathString()) > -1) {
            coord.set_locked(false);
            coord.set_clamped(false);
        }
        else if (coord.getAbsolutePathString() == get_secondary_coupled_coordinate()){
            coord.set_locked(false);
            coord.set_clamped(false);
            coord.set_prescribed(true);
        }
        else {
            coord.set_locked(true);
        }
    }

    Coordinate& coupled_coord = 
        model.updComponent<Coordinate>(get_secondary_coupled_coordinate());
    
    double start_value;
    double stop_value;

    if (model.getComponent<Coordinate>(get_secondary_coupled_coordinate()).
            getMotionType() == Coordinate::MotionType::Rotational) {

        start_value = 
            get_secondary_coupled_coordinate_start_value() * SimTK::Pi / 180;
        stop_value = 
            get_secondary_coupled_coordinate_stop_value() * SimTK::Pi / 180;
    }
    else {
        start_value = get_secondary_coupled_coordinate_start_value();
        stop_value = get_secondary_coupled_coordinate_stop_value();
    }

    //Initialize Simulation 
    //---------------------

    if (get_use_visualizer()) {
        SimTK::Visualizer& viz = model.updVisualizer().updSimbodyVisualizer();
        viz.setBackgroundColor(SimTK::White);
        viz.setShowSimTime(true);
    }
        
    //Perform Settling Simulation
    //---------------------------

    //prescribe coupled coord
    Constant settle_func = Constant(start_value);
    coupled_coord.set_prescribed_function(settle_func);

    SimTK::State state = model.initSystem();

    //prescribe muscle force
    for (Muscle& msl : model.updComponentList<Muscle>()) {
        msl.overrideActuation(state, true);
        double value = msl.getMaxIsometricForce()*0.01;
        msl.setOverrideActuation(state, value);
    }
    model.equilibrateMuscles(state);

    //setup integrator
    SimTK::CPodesIntegrator integrator(
        model.getSystem(), SimTK::CPodes::BDF, SimTK::CPodes::Newton);
    integrator.setAccuracy(get_secondary_constraint_sim_integrator_accuracy());

    if (get_secondary_constraint_sim_internal_step_limit() != -1) {
        integrator.setInternalStepLimit(
            get_secondary_constraint_sim_internal_step_limit());
    }
    SimTK::TimeStepper timestepper(model.getSystem(), integrator);

    timestepper.initialize(state);
    
    StatesTrajectory settle_states;

    double dt = 0.01;
 
    if (get_verbose() > 0) {
        std::cout << "Starting Settling Simulation."<< std::endl;
    }

    SimTK::Vector prev_sec_coord_value(_n_secondary_coord);

    double max_coord_delta = SimTK::Infinity;
    int i = 1;
    while (max_coord_delta > get_secondary_constraint_sim_settle_threshold()){
        timestepper.stepTo(i*dt);
        state = timestepper.getState();
        settle_states.append(state);
        
        if (get_verbose() > 0) {
            std::cout << std::endl;
            std::cout << "Time: " << state.getTime() << std::endl;
            std::cout << "\t\t VALUE \t\tDELTA" << std::endl;
        }

        //Compute Delta Coordinate
        max_coord_delta = 0;
        for (int k = 0; k < _n_secondary_coord; k++) {
            Coordinate& coord = model.updComponent<Coordinate>(_secondary_coord_path[k]);
            double value = coord.getValue(state);
            double delta = abs(value - prev_sec_coord_value(k));
            
            if (delta > max_coord_delta) {
                max_coord_delta = delta;
            }
            prev_sec_coord_value(k) = value;

            if (get_verbose() > 0) {
                std::cout << coord.getName() << " \t" << value << "\t" << delta <<std::endl;
            }
        }
        i++;
    }

    SimTK::Vector settled_secondary_values(_secondary_coord_path.getSize());
    SimTK::Vector settled_secondary_speeds(_secondary_coord_path.getSize());

    //Save secondardy coord values to initialize sweep simulation
    for (int c = 0; c < _secondary_coord_path.getSize(); c++) {
        std::string secondary_coord = _secondary_coord_path[c];
        Coordinate& coord = model.updComponent<Coordinate>(secondary_coord);
        settled_secondary_values.set(c, coord.getValue(state));
        settled_secondary_speeds.set(c, coord.getSpeedValue(state));
    }

    if (get_verbose() > 0) {
        std::cout << "Finished Settling Simulation in " << state.getTime() << " s." << std::endl;
        std::cout << "Starting Sweep Simulation."<< std::endl;
    }

    //Perform Sweep Simulation
    //------------------------

    //setup quadratic sweep function
    double Vx = 0;
    double Vy = start_value;
    double Px = Vx + get_secondary_constraint_sim_sweep_time();
    double Py = stop_value;
    double a = (Py - Vy) / SimTK::square(Px - Vx);

    double C1 = a;
    double C2 = -2 * a * Vx;
    double C3 = a * SimTK::square(Vx) + Vy;

    SimTK::Vector coefficients(3);
    coefficients.set(0, C1);
    coefficients.set(1, C2);
    coefficients.set(2, C3);

    PolynomialFunction sweep_func = PolynomialFunction(coefficients);
    coupled_coord.set_prescribed_function(sweep_func);

    state = model.initSystem();

    //prescribe muscle force
    for (Muscle& msl : model.updComponentList<Muscle>()) {
        msl.overrideActuation(state, true);
        double value = msl.getMaxIsometricForce()*0.01;
        msl.setOverrideActuation(state, value);
    }
    model.equilibrateMuscles(state);

    //set settled secondary coordinate values
    for (int c = 0; c < _secondary_coord_path.getSize(); c++) {
        std::string secondary_coord = _secondary_coord_path[c];
        Coordinate& coord = model.updComponent<Coordinate>(secondary_coord);
        coord.setValue(state, settled_secondary_values(c));
        coord.setSpeedValue(state, settled_secondary_speeds(c));
    }

    double sweep_start = 0;
    double sweep_stop = Px;

    int nSteps = round((sweep_stop - sweep_start) / dt);

    //Setup storage for computing constraint functions
    TimeSeriesTable q_table;
    SimTK::RowVector q_row(model.getNumCoordinates());
    std::vector<std::string> q_names;

    for (Coordinate& coord : _model.updComponentList<Coordinate>()) {
        q_names.push_back(coord.getAbsolutePathString() + "/value");
    }

    q_table.setColumnLabels(q_names);

    //setup integrator
    SimTK::CPodesIntegrator sweep_integrator(model.getSystem(), SimTK::CPodes::BDF, SimTK::CPodes::Newton);
    sweep_integrator.setAccuracy(get_secondary_constraint_sim_integrator_accuracy());
    if (get_secondary_constraint_sim_internal_step_limit() != -1) {
        sweep_integrator.setInternalStepLimit(
            get_secondary_constraint_sim_internal_step_limit());
    }
    SimTK::TimeStepper sweep_timestepper(model.getSystem(), sweep_integrator);

    sweep_timestepper.initialize(state);
    
    StatesTrajectory sweep_states;

    for (int i = 0; i <= nSteps; ++i) {

        sweep_timestepper.stepTo(sweep_start + i*dt);
        state = sweep_timestepper.getState();

        sweep_states.append(state);

        int j = 0;
        for (const auto& coord : model.getComponentList<Coordinate>()) {
            q_row(j) = coord.getValue(state);
            j++;
        }
        q_table.appendRow(state.getTime(), q_row);

        if (get_verbose() > 0) {
            std::cout << state.getTime() << std::endl;
        }
    }

    //Compute Coupled Constraint Functions
    std::vector<double> time = q_table.getIndependentColumn();

    SimTK::Vector ind_data = q_table.getDependentColumn(get_secondary_coupled_coordinate() + "/value");

    SimTK::Matrix data(time.size(), _n_secondary_coord);

for (int j = 0; j < _n_secondary_coord; ++j) {
    std::string path = _secondary_coord_path[j];
    SimTK::Vector col_data = q_table.getDependentColumn(path + "/value");

    for (int i = 0; i < nSteps; ++i) {
        data(i, j) = col_data(i);

    }
}

double ind_max = SimTK::max(ind_data);
double ind_min = SimTK::min(ind_data);

int npts = get_constraint_function_num_interpolation_points();
double step = (ind_max - ind_min) / npts;

SimTK::Vector ind_pt_data(npts);

for (int i = 0; i < npts; ++i) {
    ind_pt_data(i) = ind_min + i * step;
}

_secondary_constraint_functions.clearAndDestroy();

for (int j = 0; j < _n_secondary_coord; ++j) {
    std::string path = _secondary_coord_path[j];

    SimTK::Vector secondary_data = data(j);

    //GCVSpline* spline = new GCVSpline(5, secondary_data.nrow(), &ind_data[0], &secondary_data[0], path, -1);
    SimmSpline data_fit = SimmSpline(secondary_data.size(), &ind_data[0], &secondary_data[0]);

    SimmSpline* spline = new SimmSpline();
    spline->setName(path);

    for (int i = 0; i < npts; ++i) {
        spline->addPoint(ind_pt_data(i), data_fit.calcValue(SimTK::Vector(1, ind_pt_data(i))));
    }

    _secondary_constraint_functions.adoptAndAppend(spline);
}

//Print Secondardy Constraint Functions to file
_secondary_constraint_functions.print(get_secondary_constraint_function_file());

//Write Outputs
if (get_print_secondary_constraint_sim_results()) {
    std::cout << "Printing secondary constraint simulation results: " <<
        get_results_directory() << std::endl;

    std::string name = "secondary_constraint_sim_states";

    TimeSeriesTable settle_table = settle_states.exportToTable(model);
    settle_table.addTableMetaData("header", name);
    settle_table.addTableMetaData("nRows", 
        std::to_string(settle_table.getNumRows()));
    settle_table.addTableMetaData("nColumns", 
        std::to_string(settle_table.getNumColumns() + 1));

    TimeSeriesTable sweep_table = sweep_states.exportToTable(model);
    sweep_table.addTableMetaData("header", name);
    sweep_table.addTableMetaData("nRows", 
        std::to_string(sweep_table.getNumRows()));
    sweep_table.addTableMetaData("nColumns", 
        std::to_string(sweep_table.getNumColumns() + 1));

    std::string settle_file =
        get_results_directory() + "/" + get_results_prefix() +
        "_secondary_constraint_settle_states.sto";

    std::string sweep_file =
        get_results_directory() + "/" + get_results_prefix() +
        "_secondary_constraint_sweep_states.sto";

    STOFileAdapter sto_file_adapt;
    sto_file_adapt.write(settle_table, settle_file);
    sto_file_adapt.write(sweep_table, sweep_file);
}
}

void COMAKInverseKinematicsTool::performIK()
{
    Model model = _model;
    model.initSystem();

    try {
        _secondary_constraint_functions =
            FunctionSet(get_secondary_constraint_function_file());
    }
    catch (Exception) {
        OPENSIM_THROW(Exception, "Function file: " +
            get_secondary_constraint_function_file() + " does not exist.");

    }

    SimTK::Vector coupled_coord_default_value = SimTK::Vector(1,
        model.getComponent<Coordinate>(
            get_secondary_coupled_coordinate()).getDefaultValue());

    const std::string& secondary_coupled_coord_name = 
        model.getComponent<Coordinate>(
            get_secondary_coupled_coordinate()).getName();

        //Replace all secondary coordinates in non CustomJoint 
   for (int i = 0; i < getProperty_secondary_coordinates().size(); ++i) {
        std::string path = get_secondary_coordinates(i);
        Coordinate& coord = model.updComponent<Coordinate>(path);
        std::string coord_name = coord.getName();
        std::string ind_coord_name = model.getComponent<Coordinate>(
            get_secondary_coupled_coordinate()).getName();
        std::string joint_path = coord.getJoint().getAbsolutePathString();
        
        const Function& function = _secondary_constraint_functions.get(path);
        CoordinateCouplerConstraint* cc_constraint = new CoordinateCouplerConstraint();

        cc_constraint->setIndependentCoordinateNames(
            Array<std::string>(ind_coord_name, 1, 2));
        cc_constraint->setDependentCoordinateName(coord_name);
        cc_constraint->setFunction(function);
        cc_constraint->setName(coord_name + "_function");

        coord.setDefaultValue(function.calcValue(coupled_coord_default_value));

        model.addConstraint(cc_constraint);
   }



    //Set coordinate types
    /*for (auto& coord : model.updComponentList<Coordinate>()) {
        if (getProperty_secondary_coordinates().findIndex(coord.getAbsolutePathString()) > -1) {
            coord.set_locked(false);
            coord.set_clamped(false);
            coord.set_prescribed(false);
        }
        else if (coord.getAbsolutePathString() == get_secondary_coupled_coordinate()){
            coord.set_locked(false);
            coord.set_clamped(true);
        }
    }*/

    SimTK::State state = model.initSystem();

    if (!get_constrained_model_file().empty()) {
        model.print(get_constrained_model_file());
    }
    /*

    upd_InverseKinematicsTool().setModel(model);

    upd_InverseKinematicsTool().run();
    */

    runInverseKinematics();
}

void COMAKInverseKinematicsTool::runInverseKinematics() {

     Kinematics* kinematicsReporter = nullptr;
    try{
        // although newly loaded model will be finalized
        // there is no guarantee that the _model has not been edited/modified
        _model.finalizeFromProperties();
        _model.printBasicInfo();

        // Define reporter for output
        kinematicsReporter = new Kinematics();
        kinematicsReporter->setRecordAccelerations(false);
        kinematicsReporter->setInDegrees(true);
        _model.addAnalysis(kinematicsReporter);

        std::cout<<"Running Inverse Kinematics\n";

        // Initialize the model's underlying system and get its default state.
        SimTK::State& s = _model.initSystem();

        //Convert old Tasks to references for assembly and tracking
        MarkersReference markersReference;
        SimTK::Array_<CoordinateReference> coordinateReferences;
        // populate the references according to the setting of this Tool
        populateReferences(markersReference, coordinateReferences);

        // Determine the start time, if the provided time range is not 
        // specified then use time from marker reference.
        // Adjust the time range for the tool if the provided range exceeds
        // that of the marker data.
        SimTK::Vec2 markersValidTimeRange = markersReference.getValidTimeRange();
        double start_time = (markersValidTimeRange[0] > get_time_range(0)) ?
            markersValidTimeRange[0] : get_time_range(0);
        double final_time = (markersValidTimeRange[1] < get_time_range(1)) ?
            markersValidTimeRange[1] : get_time_range(1);

        SimTK_ASSERT2_ALWAYS(final_time >= start_time,
            "InverseKinematicsTool final time (%f) is before start time (%f).",
            final_time, start_time);

        const auto& markersTable = markersReference.getMarkerTable();
        const int start_ix = int(
            markersTable.getNearestRowIndexForTime(start_time) );
        const int final_ix = int(
            markersTable.getNearestRowIndexForTime(final_time) );
        const int Nframes = final_ix - start_ix + 1;
        const auto& times = markersTable.getIndependentColumn();

        // create the solver given the input data
        InverseKinematicsSolver ikSolver(_model, markersReference,
            coordinateReferences, get_ik_constraint_weight());
        ikSolver.setAccuracy(get_ik_accuracy());
        s.updTime() = times[start_ix];
        ikSolver.assemble(s);
        kinematicsReporter->begin(s);

        AnalysisSet& analysisSet = _model.updAnalysisSet();
        analysisSet.begin(s);
        // Get the actual number of markers the Solver is using, which
        // can be fewer than the number of references if there isn't a
        // corresponding model marker for each reference.
        int nm = ikSolver.getNumMarkersInUse();
        SimTK::Array_<double> squaredMarkerErrors(nm, 0.0);
        SimTK::Array_<Vec3> markerLocations(nm, Vec3(0));
        
        Storage *modelMarkerLocations = get_report_marker_locations() ?
            new Storage(Nframes, "ModelMarkerLocations") : nullptr;
        Storage *modelMarkerErrors = get_report_errors() ? 
            new Storage(Nframes, "ModelMarkerErrors") : nullptr;

        Stopwatch watch;

        for (int i = start_ix; i <= final_ix; ++i) {
            s.updTime() = times[i];
            ikSolver.track(s);
            // show progress line every 1000 frames so users see progress
            if (std::remainder(i - start_ix, 1000) == 0 && i != start_ix)
                std::cout << "Solved " << i - start_ix << " frames..." << std::endl;
            if(get_report_errors()){
                Array<double> markerErrors(0.0, 3);
                double totalSquaredMarkerError = 0.0;
                double maxSquaredMarkerError = 0.0;
                int worst = -1;

                ikSolver.computeCurrentSquaredMarkerErrors(squaredMarkerErrors);
                for(int j=0; j<nm; ++j){
                    totalSquaredMarkerError += squaredMarkerErrors[j];
                    if(squaredMarkerErrors[j] > maxSquaredMarkerError){
                        maxSquaredMarkerError = squaredMarkerErrors[j];
                        worst = j;
                    }
                }

                double rms = nm > 0 ? sqrt(totalSquaredMarkerError / nm) : 0;
                markerErrors.set(0, totalSquaredMarkerError); 
                markerErrors.set(1, rms);
                markerErrors.set(2, sqrt(maxSquaredMarkerError));
                modelMarkerErrors->append(s.getTime(), 3, &markerErrors[0]);

                std::cout << "Frame " << i << " (t=" << s.getTime() << "):\t"
                    << "total squared error = " << totalSquaredMarkerError
                    << ", marker error: RMS=" << rms << ", max="
                    << sqrt(maxSquaredMarkerError) << " (" 
                    << ikSolver.getMarkerNameForIndex(worst) << ")" << std::endl;
            }

            if(get_report_marker_locations()){
                ikSolver.computeCurrentMarkerLocations(markerLocations);
                Array<double> locations(0.0, 3*nm);
                for(int j=0; j<nm; ++j){
                    for(int k=0; k<3; ++k)
                        locations.set(3*j+k, markerLocations[j][k]);
                }

                modelMarkerLocations->append(s.getTime(), 3*nm, &locations[0]);

            }

            kinematicsReporter->step(s, i);
            analysisSet.step(s, i);
        }

        if (get_output_motion_file() != "" &&
                get_output_motion_file() != "Unassigned") {
            kinematicsReporter->getPositionStorage()->print(
                    get_results_directory() + "/" + get_output_motion_file());
        }
        // Remove the analysis we added to the model, this also deletes it
        _model.removeAnalysis(kinematicsReporter);

        if (modelMarkerErrors) {
            Array<std::string> labels("", 4);
            labels[0] = "time";
            labels[1] = "total_squared_error";
            labels[2] = "marker_error_RMS";
            labels[3] = "marker_error_max";

            modelMarkerErrors->setColumnLabels(labels);
            modelMarkerErrors->setName("Model Marker Errors from IK");

            IO::makeDir(get_results_directory());
            std::string errorFileName = get_results_prefix() + 
                "_ik_marker_errors";
            Storage::printResult(modelMarkerErrors, errorFileName,
                                 get_results_directory(), -1, ".sto");

            delete modelMarkerErrors;
        }

        if(modelMarkerLocations){
            Array<std::string> labels("", 3*nm+1);
            labels[0] = "time";
            Array<std::string> XYZ("", 3*nm);
            XYZ[0] = "_tx"; XYZ[1] = "_ty"; XYZ[2] = "_tz";

            for(int j=0; j<nm; ++j){
                for(int k=0; k<3; ++k)
                    labels.set(3*j+k+1, ikSolver.getMarkerNameForIndex(j)+XYZ[k]);
            }
            modelMarkerLocations->setColumnLabels(labels);
            modelMarkerLocations->setName("Model Marker Locations from IK");
    
            IO::makeDir(get_results_directory());
            std::string markerFileName = get_results_prefix() + 
                "_ik_model_marker_locations";
            Storage::printResult(modelMarkerLocations, markerFileName,
                                 get_results_directory(), -1, ".sto");

            delete modelMarkerLocations;
        }

        std::cout << "InverseKinematicsTool completed " << Nframes << " frames in "
            << watch.getElapsedTimeFormatted() << "\n" <<std::endl;
    }
    catch (const std::exception& ex) {
        std::cout << "InverseKinematicsTool Failed: " << ex.what() << std::endl;
        // If failure happened after kinematicsReporter was added, make sure to cleanup
        if (kinematicsReporter!= nullptr)
            _model.removeAnalysis(kinematicsReporter);
        throw (Exception("InverseKinematicsTool Failed, "
            "please see messages window for details..."));
    }
}

void COMAKInverseKinematicsTool::populateReferences(MarkersReference& markersReference,
    SimTK::Array_<CoordinateReference>&coordinateReferences) const
{
    FunctionSet *coordFunctions = NULL;
    // Load the coordinate data
    // bool haveCoordinateFile = false;
    if (get_coordinate_file() != "" && get_coordinate_file() != "Unassigned") {
        Storage coordinateValues(get_coordinate_file());
        // Convert degrees to radian (TODO: this needs to have a check that the storage is, in fact, in degrees!)
        _model.getSimbodyEngine().convertDegreesToRadians(coordinateValues);
        // haveCoordinateFile = true;
        coordFunctions = new GCVSplineSet(5, &coordinateValues);
    }

    Set<MarkerWeight> markerWeights;
    // Loop through old "IKTaskSet" and assign weights to the coordinate and marker references
    // For coordinates, create the functions for coordinate reference values
    int index = 0;
    for (int i = 0; i < get_IKTaskSet().getSize(); i++) {
        if (!get_IKTaskSet()[i].getApply()) continue;
        if (IKCoordinateTask *coordTask = dynamic_cast<IKCoordinateTask *>(&get_IKTaskSet()[i])) {
            CoordinateReference *coordRef = NULL;
            if (coordTask->getValueType() == IKCoordinateTask::FromFile) {
                if (!coordFunctions)
                    throw Exception("InverseKinematicsTool: value for coordinate " + coordTask->getName() + " not found.");

                index = coordFunctions->getIndex(coordTask->getName(), index);
                if (index >= 0) {
                    coordRef = new CoordinateReference(coordTask->getName(), coordFunctions->get(index));
                }
            }
            else if ((coordTask->getValueType() == IKCoordinateTask::ManualValue)) {
                Constant reference(Constant(coordTask->getValue()));
                coordRef = new CoordinateReference(coordTask->getName(), reference);
            }
            else { // assume it should be held at its default value
                double value = _model.getCoordinateSet().get(coordTask->getName()).getDefaultValue();
                Constant reference = Constant(value);
                coordRef = new CoordinateReference(coordTask->getName(), reference);
            }

            if (coordRef == NULL)
                throw Exception("InverseKinematicsTool: value for coordinate " + coordTask->getName() + " not found.");
            else
                coordRef->setWeight(coordTask->getWeight());

            coordinateReferences.push_back(*coordRef);
        }
        else if (IKMarkerTask *markerTask = dynamic_cast<IKMarkerTask *>(&get_IKTaskSet()[i])) {
            if (markerTask->getApply()) {
                // Only track markers that have a task and it is "applied"
                markerWeights.adoptAndAppend(
                    new MarkerWeight(markerTask->getName(), markerTask->getWeight()));
            }
        }
    }

    //Read in the marker data file and set the weights for associated markers.
    //Markers in the model and the marker file but not in the markerWeights are
    //ignored
    markersReference.initializeFromMarkersFile(get_marker_file(), markerWeights);
}