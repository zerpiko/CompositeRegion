#include <deal.II/base/multithread_info.h>  
#include <deal.II/base/parameter_handler.h>

#include <deal.II/dofs/dof_handler.h> 
#include <deal.II/dofs/dof_tools.h>

#include <deal.II/fe/fe_q.h>
#include <deal.II/fe/fe_values.h>

#include <deal.II/grid/tria.h>
#include <deal.II/grid/grid_generator.h>

#include <deal.II/lac/vector.h>
#include <deal.II/lac/full_matrix.h>
#include <deal.II/lac/dynamic_sparsity_pattern.h>
#include <deal.II/lac/sparse_matrix.h>   
#include <deal.II/lac/solver_cg.h>
#include <deal.II/lac/precondition.h>

#include <deal.II/numerics/vector_tools.h>
#include <deal.II/numerics/matrix_tools.h>
#include <deal.II/numerics/data_out.h>

#include <fstream>
#include <iostream>
#include <math.h>
#include <sstream> 
#include <string>
#include <vector>
#include <tuple>
#include <DataTools.h>
#include <Names.h>

class Material
{
public:
  Material(std::string material_name);
  Material(double solids_thermal_conductivity_,
	   double solids_density_,
	   double solids_specific_heat_capacity_);
  virtual double thermal_conductivity();
  virtual double density();
  virtual double specific_heat_capacity();
protected:
  double solids_thermal_conductivity;
  double solids_density;
  double solids_specific_heat_capacity;
  /*
   * material name - thermal conductivity [W/mK] - density [kg/m3] - specific heat capacity [J/kgK]
   */
  std::map<std::string,std::array<double,3> > material_data
  ={
    {"dummy_1"    ,{1.  , 2.     ,    3.  }},
    {"dummy_2"    ,{4.  , 5.     ,    6.  }},
    {"quartz_1"   ,{8.79, 2660.00, 2010.00}},
    {"pvc_1"      ,{0.22, 1200.00, 1200.00}},
    {"glass_beads",{0.80, 2500.00, 1175.00}},
    {"pvc_2"      ,{0.16, 1440.00,  900.00}}
  };
};

Material::Material(std::string material_name_)
{
  solids_thermal_conductivity=0.;
  solids_density=0.;
  solids_specific_heat_capacity=0.;
  
  std::map<std::string,std::array<double,3> >::iterator it;
  it = material_data.find(material_name_);
  if(it!=material_data.end())
    {
      solids_thermal_conductivity=it->second[0];
      solids_density=it->second[1];
      solids_specific_heat_capacity=it->second[2];
    }
  else
    {
      std::cout << "Error. Material not found\n";
    }
}

Material::Material(double solids_thermal_conductivity_,
		   double solids_density_,
		   double solids_specific_heat_capacity_)
  :
  solids_thermal_conductivity(solids_thermal_conductivity_),
  solids_density(solids_density_),
  solids_specific_heat_capacity(solids_specific_heat_capacity_)
{
  
}

double Material::thermal_conductivity()
{
  return solids_thermal_conductivity;
}

double Material::density()
{
  return solids_density;
}

double Material::specific_heat_capacity()
{
  return solids_specific_heat_capacity;
}
/*
* This class defines a material that in principle can be composed of three 
* constituents: solid, liquid and gas.
*/
class PorousMaterial:public Material
{
public:
  PorousMaterial(std::string material_name_,
		 double porosity_=0.,
		 double degree_of_saturation_=0.);

  PorousMaterial(double solids_thermal_conductivity_,
  		 double solids_density_,
  		 double solids_specific_heat_capacity_,
		 double porosity_=0.,
		 double degree_of_saturation_=0.);

  virtual double thermal_conductivity();
  virtual double thermal_conductivity(const std::string relationship);

  void set_liquid_properties(double liquid_thermal_conductivity_,
			     double liquid_density_,
			     double liquid_specific_heat_capacity_);
  void set_gas_properties(double gas_thermal_conductivity_,
			  double gas_density_,
			  double gas_specific_heat_capacity_);
  void set_ice_properties(double ice_thermal_conductivity_,
			  double ice_density_,
			  double ice_specific_heat_capacity_);
  void set_freezing_properties(double freezing_point_,
			       double coefficient_alpha_,
			       double reference_temperature_,
			       double latent_heat_of_fusion_);
  double degree_of_saturation_ice(const double temperature);
  double degree_of_saturation_ice_derivative(const double temperature);
  double volumetric_heat_capacity(const double temperature);
  double thermal_energy(const double temperature);
  
private:
  void init_liquid_gas_and_ice_parameters();

  void init_freezing_parameters();
  
  double porosity;
  double degree_of_saturation;
  
  double liquid_thermal_conductivity;
  double liquid_density;
  double liquid_specific_heat_capacity;

  double gas_thermal_conductivity;
  double gas_density;
  double gas_specific_heat_capacity;

  double ice_thermal_conductivity;
  double ice_density;
  double ice_specific_heat_capacity;
  
  double freezing_point;
  double coefficient_alpha;
  double reference_temperature;
  double latent_heat_of_fusion;
};

PorousMaterial::PorousMaterial(std::string material_name_,
			       double porosity_,
			       double degree_of_saturation_)
  :Material(material_name_),
   porosity(porosity_),
   degree_of_saturation(degree_of_saturation_)
{
  init_liquid_gas_and_ice_parameters();
  init_freezing_parameters();
}

PorousMaterial::PorousMaterial(double solids_thermal_conductivity_,
			       double solids_density_,
			       double solids_specific_heat_capacity_,
			       double porosity_,
			       double degree_of_saturation_)
  :Material(solids_thermal_conductivity_,
	    solids_density_,
	    solids_specific_heat_capacity_),
   porosity(porosity_),
   degree_of_saturation(degree_of_saturation_)
{
  init_liquid_gas_and_ice_parameters();
  init_freezing_parameters();
}

void PorousMaterial::init_liquid_gas_and_ice_parameters()
{
  liquid_thermal_conductivity  =   0.57;// [W/mK]
  liquid_density               =1000.00;// [kg/m3]
  liquid_specific_heat_capacity=4186.00; // [J/kgK]
  
  gas_thermal_conductivity  =0.025; // [W/mK]
  gas_density               =1.25 ; // [kg/m3]
  gas_specific_heat_capacity=1.256; // [J/kgK]

  ice_thermal_conductivity  =   2.22; //(@  0C) [W/mK]
  ice_density               = 920.00; //(@-30C)[kg/m3]
  ice_specific_heat_capacity=1844.00; //(@-30C)[J/kgK]
}

void PorousMaterial::init_freezing_parameters()
{
  freezing_point       =     0.0; // [C]
  coefficient_alpha    =    -5.0; // [nondimensional?]
  reference_temperature=     0.0; // [C]
  latent_heat_of_fusion=334000.0; // [J/K]
}

void PorousMaterial::set_liquid_properties(double liquid_thermal_conductivity_,
					   double liquid_density_,
					   double liquid_specific_heat_capacity_)
{
  liquid_thermal_conductivity=liquid_thermal_conductivity_;
  liquid_density=liquid_density_;
  liquid_specific_heat_capacity=liquid_specific_heat_capacity_;
}
void PorousMaterial::set_gas_properties(double gas_thermal_conductivity_,
					double gas_density_,
					double gas_specific_heat_capacity_)
{
  gas_thermal_conductivity=gas_thermal_conductivity_;
  gas_density=gas_density_;
  gas_specific_heat_capacity=gas_specific_heat_capacity_;
}

void PorousMaterial::set_ice_properties(double ice_thermal_conductivity_,
					double ice_density_,
					double ice_specific_heat_capacity_)
{
  ice_thermal_conductivity=ice_thermal_conductivity_;
  ice_density=ice_density_;
  ice_specific_heat_capacity=ice_specific_heat_capacity_;
}

void PorousMaterial::set_freezing_properties(double freezing_point_,
					     double coefficient_alpha_,
					     double reference_temperature_,
					     double latent_heat_of_fusion_)
{
  freezing_point=freezing_point_;
  coefficient_alpha=coefficient_alpha_;
  reference_temperature=reference_temperature_;
  latent_heat_of_fusion=latent_heat_of_fusion_;
}

double PorousMaterial::thermal_conductivity()
{
  return thermal_conductivity("donazzi");
}

double PorousMaterial::thermal_conductivity(const std::string relationship)
{
  double thermal_conductivity=0.;
  if (relationship.compare("donazzi")==0)
    {
      /*
       * Estimation of thermal conductivity from Donazzi (1979)
       * Donazzi neglects the contribution of air. But the formulation
       * is applicable to unsaturated soils because it includes degree
       * of saturation.
       * */
      thermal_conductivity
	=1./(pow(1./liquid_thermal_conductivity,porosity)*
	     pow(1./solids_thermal_conductivity,1.-porosity)*
	     exp(3.08*(1.-degree_of_saturation)*porosity));
    }
  else if (relationship.compare("haigh")==0)
    {
      /*
       * Estimation of thermal conductivity from Haigh (2012)
       *
       * */
      double void_ratio=porosity/(1.-porosity);
      double E=(2.*void_ratio-1.)/3.;//xi
      double B=(1./3.)*acos((2.*(1.+3.*E)*(1.-degree_of_saturation)-pow(1.+E,3.))/pow(1.+E,3.));
      double X=0.5*(1.+E)*(1.+cos(B)-pow(3.,0.5)*sin(B));
      double a_w=liquid_thermal_conductivity/solids_thermal_conductivity;
      double a_a=gas_thermal_conductivity/solids_thermal_conductivity;

      thermal_conductivity
	=1.58*solids_thermal_conductivity
	*(2.*pow(1.+E,2.)*
	  ((a_w/pow(1.-a_w,2.))*log(((1+E)+(a_w-1.)*X)/(E+a_w))+
	   (a_a/pow(1.-a_a,2.))*log((1.+E)/((1+E)+(a_a-1.)*X)))
	  +(2.*(1.+E)/((1.-a_w)*(1.-a_a)))*((a_w-a_a)*X-(1.-a_a)*a_w));
    }
  else if (relationship.compare("bulk")==0)
    {
      /*
       * This only returns the same value that is given as input in the input
       * file. Is meant to let you test quickly values for this parameter or
       * for materials that are not porous (like plastics).
       * */
      thermal_conductivity
	=solids_thermal_conductivity;
    }
  else
    {
      std::cout << "Error. Wrong thermal conductivity relationship"
	" requested not currently implemented.\n";
      throw -1;
    }
  return thermal_conductivity;
}

double PorousMaterial::degree_of_saturation_ice(const double temperature)
{
  if (temperature<=freezing_point)
    return 1.-pow(1.-(temperature-freezing_point),coefficient_alpha);
  else
    return 0.;
}

double PorousMaterial::degree_of_saturation_ice_derivative(const double temperature)
{
  if (temperature<=freezing_point)
    return coefficient_alpha*pow(1.-(temperature-freezing_point),coefficient_alpha-1.);
  else
    return 0.;
}

double PorousMaterial::volumetric_heat_capacity(const double temperature)
{
  double Hc=
    (1.-degree_of_saturation_ice(temperature))*
    porosity*degree_of_saturation*liquid_specific_heat_capacity*liquid_density
    +porosity*gas_specific_heat_capacity*gas_density*(1.-degree_of_saturation)
    +solids_specific_heat_capacity*solids_density*(1.-porosity)
    +porosity*degree_of_saturation*degree_of_saturation_ice(temperature)*
    ice_specific_heat_capacity*ice_density;
  double a=
    (temperature-reference_temperature)*
    (degree_of_saturation*ice_density*ice_specific_heat_capacity
     -degree_of_saturation*liquid_density*liquid_specific_heat_capacity);
  double b=
    degree_of_saturation*ice_density*latent_heat_of_fusion;
  return Hc+porosity*degree_of_saturation_ice_derivative(temperature)*(a-b);;
}

double PorousMaterial::thermal_energy(const double temperature)
{
  double Hc=
    (1.-degree_of_saturation_ice(temperature))*
    porosity*degree_of_saturation*liquid_specific_heat_capacity*liquid_density
    +porosity*gas_specific_heat_capacity*gas_density*(1.-degree_of_saturation)
    +solids_specific_heat_capacity*solids_density*(1.-porosity)
    +porosity*degree_of_saturation*degree_of_saturation_ice(temperature)*
    ice_specific_heat_capacity*ice_density;
  return (Hc*(temperature-reference_temperature)
	  -latent_heat_of_fusion*porosity*degree_of_saturation*
	  degree_of_saturation_ice(temperature)*ice_density);
}

//--------------------------------------------------------------------------------------
namespace TRL
{
  using namespace dealii;
#include "InitialValue.h"
#include "parameters.h"

  template <int dim>
  class Heat_Pipe
  {
  public:
    Heat_Pipe(int argc, char *argv[]);
    ~Heat_Pipe();
    void run();

  private:
    void read_grid_temperature();
    void setup_system_temperature();
    void assemble_system_temperature();
    void solve_temperature();
    void initial_condition_temperature();

    void output_results ();
    void fill_output_vectors();
    void update_met_data ();

    void material_data(const double x/*(m)*/,
		       const double temperature/*(C)*/,
		       const double cell_diameter/*(m)*/,
		       double &thermal_conductivity/*(W/mK)*/,
		       double &total_volumetric_heat_capacity/*(J/m3K)*/,
		       double &thermal_energy/*J*/,
		       double &ice_saturation);

    double thermal_conductivity_relations(const std::string relationship,
					  const double porosity,
					  const double degree_of_saturation,
					  double thermal_conductivity_solids);

    double thermal_losses(const double temperature_gradient/*(m)*/);
    //double snow_surface_heat_flux(double surface_temperature); //(W/m2)

    Triangulation<dim>   triangulation;
    DoFHandler<dim>      dof_handler;
    FE_Q<dim>            fe;

    ConstraintMatrix     hanging_node_constraints;

    SparsityPattern      sparsity_pattern;

    SparseMatrix<double> system_matrix;
    SparseMatrix<double> mass_matrix;
    SparseMatrix<double> laplace_matrix_new;
    SparseMatrix<double> laplace_matrix_old;
    Vector<double>       system_rhs;
    Vector<double>       solution;
    Vector<double>       old_solution;

    unsigned int timestep_number_max;
    unsigned int timestep_number;
    double       time;
    double       time_step;
    double       time_max;
    double       theta_temperature;

    Threads::Mutex assembler_lock;
    Parameters::AllParameters<dim>  parameters;

    //std::vector< std::vector<int> >    date_and_time;
    std::vector< std::vector<double> > met_data;
    std::vector<std::vector<double> > interpolated_temperature_surface;
    std::vector<std::vector<double> > interpolated_temperature_room;
    std::vector< std::vector<double> > depths_coordinates;
    std::vector< std::vector<double> > temperatures_at_points;
    std::vector< std::vector<double> > point_source_magnitudes;
    double old_room_temperature, new_room_temperature;
    double old_surface_temperature, new_surface_temperature;
    double old_point_source_magnitude, new_point_source_magnitude;
    double column_thermal_energy;
    double thermal_conductivity_liquids;
    double thermal_conductivity_air;

    std::ofstream output_file;
    /*
     * string "material_name"
     * double "porosity"
     * double "degree_of_saturation"
     * string "relationship"
     */
    std::vector<std::tuple<std::string,double,double,std::string> > layer_data;
  };

  template<int dim>
  Heat_Pipe<dim>::Heat_Pipe(int argc, char *argv[])
    :
    dof_handler(triangulation),
    fe(1)
  {
    if (argc!=2)
      {
	std::cout << "Wrong number of input arguments.\n"
		  << "Number of arguments passed: " << argc << "\n"
		  << "Number of arguments expected: 2\n"
		  << "Missing input file?\n" << std::endl;
	throw 1;
      }

    const std::string input_filename = argv[1];
    std::cout << "parameter file: " << input_filename << "\n";

    ParameterHandler prm;
    Parameters::AllParameters<dim>::declare_parameters (prm);

    std::ifstream inFile;
    inFile.open(input_filename.c_str());
    
    //prm.read_input(input_filename);
    prm.parse_input(inFile,input_filename);
    
    parameters.parse_parameters (prm);

    theta_temperature   = parameters.theta;
    timestep_number_max = parameters.timestep_number_max;
    time_step           = parameters.time_step;
    time_max            = time_step*timestep_number_max;

    thermal_conductivity_liquids   = parameters.thermal_conductivity_liquids;
    thermal_conductivity_air       = parameters.thermal_conductivity_air;
    /*
      We want to read the file containing the coordinates
      of the points we are interested in. We will store
      them in a vector and use them later to extract data
      from the solution vector and do some calculations
      (e.g. stored thermal energy).
    */
    std::vector< std::vector<int> >    dummy_matrix;
    std::vector< std::string >         filenames;
    filenames.push_back(parameters.depths_file);

    DataTools data_tools;
    data_tools.read_data (filenames,
			  depths_coordinates);

    std::cout << "Available depth coordinate entries: "
	      << depths_coordinates.size() << std::endl
	      << "Depth coordinates (m):\n"
	      << "\tX\tY\tZ\n";
    for (unsigned int i=0; i<depths_coordinates.size(); i++)
      {
	for (unsigned int j=0; j<depths_coordinates[i].size(); j++)
	  std::cout << "\t" << depths_coordinates[i][j];
	std::cout << "\n";
      }

    std::string output_filename=parameters.output_file;
    remove(output_filename.c_str());
    output_file.open(output_filename.c_str(),std::ios::app);
    if (!output_file.is_open()) //some error with the file
      {
    	std::cout << "Error opening output data file\n";
    	throw 1;
      }

    if (parameters.boundary_condition_top.compare("first")!=0 &&
    	parameters.boundary_condition_top.compare("second")!=0 &&
	parameters.boundary_condition_top.compare("third")!=0 )
      {
    	std::cout << "\n\n\tError, wrong top boundary condition type\n\n";
      }

    old_room_temperature       = 0.;
    new_room_temperature       = 0.;
    old_surface_temperature    = 0.;
    new_surface_temperature    = 0.;
    old_point_source_magnitude = 0.;
    new_point_source_magnitude = 0.;
    time=0.;
    timestep_number=0;
    column_thermal_energy=0.;

    layer_data
      .push_back(std::tuple<std::string,double,double,std::string>
		 (parameters.material_0_name,
		  parameters.material_0_porosity,
		  parameters.material_0_degree_of_saturation,
		  parameters.material_0_thermal_conductivity_relationship));
    layer_data
      .push_back(std::tuple<std::string,double,double,std::string>
		 (parameters.material_1_name,
		  parameters.material_1_porosity,
		  parameters.material_1_degree_of_saturation,
		  parameters.material_1_thermal_conductivity_relationship));
    layer_data
      .push_back(std::tuple<std::string,double,double,std::string>
		 (parameters.material_2_name,
		  parameters.material_2_porosity,
		  parameters.material_2_degree_of_saturation,
		  parameters.material_2_thermal_conductivity_relationship));
    layer_data
      .push_back(std::tuple<std::string,double,double,std::string>
		 (parameters.material_3_name,
		  parameters.material_3_porosity,
		  parameters.material_3_degree_of_saturation,
		  parameters.material_3_thermal_conductivity_relationship));
     layer_data
      .push_back(std::tuple<std::string,double,double,std::string>
		 (parameters.material_4_name,
		  parameters.material_4_porosity,
		  parameters.material_4_degree_of_saturation,
		  parameters.material_4_thermal_conductivity_relationship));
  }

  template<int dim>
  Heat_Pipe<dim>::~Heat_Pipe ()
  {
    dof_handler.clear ();
  }

  template <int dim>
  void Heat_Pipe<dim>::read_grid_temperature()
  {
    GridGenerator::hyper_cube (triangulation,-1.*parameters.domain_size, 0);
    triangulation.refine_global (parameters.refinement_level);
    dof_handler.distribute_dofs (fe);
  }

  template <int dim>
  void Heat_Pipe<dim>::material_data(const double cell_center,
				     const double cell_temperature,
				     const double cell_diameter,
				     double &thermal_conductivity,
				     double &total_volumetric_heat_capacity,
				     double &thermal_energy,
				     double &ice_saturation)
  {
    /*
     * We are assuming that each layer is composed of three fractions: solid, liquid, frozen liquid,
     * and gas. At the moment, the assumption is that liquid is water, frozen liquid is ice and gas
     * is air for all layers. Solids can vary, in out particular case, at least one layer has a
     * different kind of solids.
     *
     * Porosity and degree of saturation is also layer dependent. Thus, fractions are layer
     * dependent.
     *
     * Thermal properties can be considered equal for liquids and gas fractions but not for solids
     * across the layers.
     *
     * NOTE that at the moment there are two layers composed of a single element (plastic lids)
     * the thermal properties of these layers are calculated in a different way until we think of
     * a way of homogenize the code.
     *
     * The function receives thermal conductivity of solids in the layer of interest.
     * */
    /*
     * These variables are assumed to be constants. That's why we defined them inside the function.
     * */
    unsigned int layer_number=0;
    if (cell_center>-1.*(parameters.material_0_depth+parameters.material_0_thickness))/*layer 0*/
      {
	layer_number=0;
      }
    else if (cell_center<=-1.*parameters.material_1_depth &&
	     cell_center>-1.*(parameters.material_1_depth+parameters.material_1_thickness))/*layer 1*/
      {
	layer_number=1;
      }
    else if (cell_center<=-1.* parameters.material_2_depth &&
	     cell_center>-1.*(parameters.material_2_depth+parameters.material_2_thickness))/*layer 2*/
      {
	layer_number=2;
      }
    else if (cell_center<=-1.* parameters.material_3_depth &&
	     cell_center>-1.*(parameters.material_3_depth+parameters.material_3_thickness))/*layer 3*/
      {
	layer_number=3;
      }
    else if (cell_center<=-1.*parameters.material_4_depth)/*layer 4*/
      {
	layer_number=4;
      }
    else
      {
	std::cout << "Error. Cell centre not found." << std::endl;
	throw -1;
      }

    std::string material_name  =std::get<0>(layer_data[layer_number]);
    double porosity            =std::get<1>(layer_data[layer_number]);
    double degree_of_saturation=std::get<2>(layer_data[layer_number]);
    std::string relationship   =std::get<3>(layer_data[layer_number]);

    PorousMaterial *porous_material = new PorousMaterial(material_name,
							 porosity,
							 degree_of_saturation);
    thermal_conductivity
      =porous_material->thermal_conductivity(relationship);
    total_volumetric_heat_capacity
      =porous_material->volumetric_heat_capacity(cell_temperature);
    ice_saturation
      =porous_material->degree_of_saturation_ice(cell_temperature);
    thermal_energy
      =cell_diameter*
      porous_material->thermal_energy((   theta_temperature)*VectorTools::point_value(dof_handler,    solution,Point<dim>(cell_center))+(1.-theta_temperature)*VectorTools::point_value(dof_handler,old_solution,Point<dim>(cell_center)));

    if (thermal_conductivity<0. || total_volumetric_heat_capacity<0.)
      {
	std::cout << "thermal_conductivity: " << thermal_conductivity << "\t"
		  << "total_volumetric_heat_capacity: "<< total_volumetric_heat_capacity << "\t"
		  << "cell temperature: " << cell_temperature << "\t"
		  << "ice content: " << ice_saturation << "\t"
		  << std::endl;
	throw -1;
      }
  }

  template <int dim>
  double Heat_Pipe<dim>::thermal_losses(double const temperature_gradient)
  {
    /*
     * At the moment is the convective coefficient is read from the input
     * file, but this function could include the equations used to estimate
     * this coefficient.
     * */
    double convective_coefficient = parameters.heat_loss_factor; // W/m3K
    return (-1.*convective_coefficient*(temperature_gradient)); //(W/m3)
  }

  //  template <int dim>
  //  double Heat_Pipe<dim>::snow_surface_heat_flux(double surface_temperature)
  //  {
  // double sensible_flux=10.; // CANNOT BE ZERO
  // const double gravity_acceleration = 9.81; //m/s

  // double Obukhov_length=
  // 		  (-1.*parameters.virtual_temperature*parameters.density_air*parameters.specific_heat_capacity_air*pow(parameters.friction_velocity,3))
  // 		  /(parameters.von_Karman_constant*gravity_acceleration*sensible_flux);

  // double zeta=
  // 		  parameters.reference_height/Obukhov_length;

  // double momentum_stratification_factor=
  // 		  1.+(0.7*zeta)+(0.75*zeta*(exp(6.-0.35*zeta),(-0.35*zeta)));

  // double humidity_stratification_factor=
  // 		  momentum_stratification_factor;

  // double sensible_coefficient=
  // 		  pow(parameters.von_Karman_constant,2)/
  // 		  ((log(parameters.reference_height/parameters.roughness_length            )-momentum_stratification_factor)
  // 				  *(log(parameters.reference_height/parameters.temperature_roughness_length)-humidity_stratification_factor));

  // sensible_flux=
  // 		  parameters.density_air*parameters.specific_heat_capacity_air*sensible_coefficient*parameters.air_speed
  // 		  *(parameters.height_temperature-surface_temperature/*parameters.surface_temperature*/);

  // /*
  //  * Are we considering moisture flow?? In principle we should, this would take into account the formation of snow
  //  * through condensation of water on the soil surface and eventual evaporation (thawing?). But I thought that we
  //  * were going to start with an assumption of sudden appearance of snow on the soil surface without worrying
  //  * about how it got there... we need to clarify this
  //  * */
  // double latent_coefficient=
  // 		  sensible_coefficient;

  // double latent_flux=
  // 		  parameters.density_air*parameters.vaporization_latent_heat*latent_coefficient*parameters.air_speed
  // 		  *(parameters.height_humidity-parameters.surface_humidity);
  // //-----------------------------------------------------------------------------------------------------------------

  // double short_wave_flux=
  // 		  parameters.short_wave_radiation*(1.-parameters.albedo);

  // double long_wave_flux=
  // 		  parameters.Boltzmann_constant*
  // 		  (parameters.surface_emissivity*pow(parameters.surface_temperature,4)
  //         -parameters.atmospher_emissivity*pow(parameters.height_temperature,4));

  // double conductance_flux=
  // 		  parameters.material_0_thermal_conductivity*parameters.surface_temperature/parameters.material_0_depth;

  // return (latent_flux+old_sensible_flux_iteration+short_wave_flux+long_wave_flux+conductance_flux);
  //  }

  template <int dim>
  void Heat_Pipe<dim>::setup_system_temperature()
  {
    hanging_node_constraints.clear ();
    DoFTools::make_hanging_node_constraints (dof_handler,
					     hanging_node_constraints);
    hanging_node_constraints.close ();

    DynamicSparsityPattern csp(dof_handler.n_dofs(),
			       dof_handler.n_dofs());

    DoFTools::make_sparsity_pattern (dof_handler, csp);

    hanging_node_constraints.condense (csp);
    sparsity_pattern.copy_from (csp);
  }

  template <int dim>
  void Heat_Pipe<dim>::assemble_system_temperature()
  {
    system_rhs.reinit         (dof_handler.n_dofs());
    system_matrix.reinit      (sparsity_pattern);
    mass_matrix.reinit        (sparsity_pattern);
    laplace_matrix_new.reinit (sparsity_pattern);
    laplace_matrix_old.reinit (sparsity_pattern);

    QGauss<dim>   quadrature_formula(3);
    const QGauss<dim-1>   face_quadrature_formula(3);
    FEValues<dim> fe_values(fe, quadrature_formula,
			    update_values | update_gradients |
			    update_quadrature_points | update_JxW_values);
    FEFaceValues<dim> fe_face_values(fe, face_quadrature_formula,
				     update_values | update_gradients |
				     update_quadrature_points | update_JxW_values);
	  
    const unsigned int dofs_per_cell   = fe.dofs_per_cell;
    const unsigned int n_q_points      = quadrature_formula.size();
    const unsigned int n_face_q_points = face_quadrature_formula.size();
	  
    FullMatrix<double> cell_mass_matrix        (dofs_per_cell,dofs_per_cell);
    FullMatrix<double> cell_laplace_matrix_new (dofs_per_cell,dofs_per_cell);
    FullMatrix<double> cell_laplace_matrix_old (dofs_per_cell,dofs_per_cell);
    Vector<double>     cell_rhs                (dofs_per_cell);
	  
    std::vector<unsigned int> local_dof_indices (fe.dofs_per_cell);
    column_thermal_energy= 0.;
	  
    typename DoFHandler<dim>::active_cell_iterator
      cell = dof_handler.begin_active(),
      endc = dof_handler.end();
    for (; cell!=endc; ++cell)
      {
	fe_values.reinit (cell);
	cell_mass_matrix        = 0;
	cell_laplace_matrix_new = 0;
	cell_laplace_matrix_old = 0;
	cell_rhs                = 0;

	double average_cell_temperature=
	  (   theta_temperature)*VectorTools::point_value(dof_handler,    solution,Point<dim>(cell->center()[0]))+
	  (1.-theta_temperature)*VectorTools::point_value(dof_handler,old_solution,Point<dim>(cell->center()[0]));

	double old_cell_heat_loss = thermal_losses(average_cell_temperature-old_room_temperature);
	double new_cell_heat_loss = thermal_losses(average_cell_temperature-new_room_temperature);

	double cell_thermal_conductivity          = -1.E10;
	double cell_total_volumetric_heat_capacity= -1.E10;
	double cell_thermal_energy                = -1.E10;
	double cell_ice_saturation                = -1.E10;
	material_data(cell->center()[0],average_cell_temperature,cell->diameter(),
		      cell_thermal_conductivity,cell_total_volumetric_heat_capacity,
		      cell_thermal_energy,cell_ice_saturation);

	column_thermal_energy+=cell_thermal_energy;

	for (unsigned int q_point=0; q_point<n_q_points; ++q_point)
	  {
	    /*
	     * Here is were we assemble the matrices and vectors that appear after
	     * we discretize the problem in space and time using the finite element
	     * method. And here is also were we need to put any sinks or sources we
	     * want to implement. For the moment the magnitude of the source is user
	     * defined (by an external file). But at some point this must be
	     * calculated in a more appropiated way.
	     */
	    for (unsigned int i=0; i<dofs_per_cell; ++i)
	      {
		for (unsigned int j=0; j<dofs_per_cell; ++j)
		  {
		    cell_mass_matrix(i,j)+=
		      cell_total_volumetric_heat_capacity*
		      fe_values.shape_value(i,q_point) *
		      fe_values.shape_value(j,q_point) *
		      fe_values.JxW(q_point);
		    cell_laplace_matrix_new(i,j)+=
		      cell_thermal_conductivity *
		      fe_values.shape_grad(i,q_point) *
		      fe_values.shape_grad(j,q_point) *
		      fe_values.JxW(q_point);
		    cell_laplace_matrix_old(i,j)+=
		      cell_thermal_conductivity *
		      fe_values.shape_grad(i,q_point) *
		      fe_values.shape_grad(j,q_point) *
		      fe_values.JxW(q_point);
		  }
		cell_rhs(i)+=
		  new_cell_heat_loss*theta_temperature*time_step*
		  fe_values.shape_value(i,q_point) *
		  fe_values.JxW(q_point)
		  +
		  old_cell_heat_loss*(1.-theta_temperature)*time_step*
		  fe_values.shape_value(i,q_point) *
		  fe_values.JxW(q_point);
	      }
	  }

	if (parameters.boundary_condition_top.compare("second")==0 ||
	    parameters.boundary_condition_top.compare("third")==0)
	  {
	    double convective_coefficient=10.;
	    double top_outbound_convective_coefficient=0.;
	    double top_inbound_heat_flux_new=0.;
	    double top_inbound_heat_flux_old=0.;
	    if (parameters.boundary_condition_top.compare("second")==0)
	      {
		top_inbound_heat_flux_new=-100.;
		top_inbound_heat_flux_old=-100.;
	      }
	    else if (parameters.boundary_condition_top.compare("third")==0)
	      {
		top_outbound_convective_coefficient=convective_coefficient;
		top_inbound_heat_flux_new=convective_coefficient*new_surface_temperature;
		top_inbound_heat_flux_old=convective_coefficient*old_surface_temperature;
	      }

	    for (unsigned int face=0; face<GeometryInfo<dim>::faces_per_cell; ++face)
	      if (cell->face(face)->at_boundary() &&
		  fabs(cell->face(face)->center()[0]+0.)<0.0001)
		{
		  fe_face_values.reinit (cell, face);
		  for (unsigned int q_face_point=0; q_face_point<n_face_q_points; ++q_face_point)
		    {
		      for (unsigned int i=0; i<dofs_per_cell; ++i)
			{
			  if(parameters.boundary_condition_top.compare("third")==0)
			    {
			      for (unsigned int j=0; j<dofs_per_cell; ++j)
				{
				  cell_laplace_matrix_new (i,j)+=
				    top_outbound_convective_coefficient *
				    fe_face_values.shape_value (i,q_face_point) *
				    fe_face_values.shape_value (j,q_face_point) *
				    fe_face_values.JxW         (q_face_point);
				  cell_laplace_matrix_old (i,j)+=
				    top_outbound_convective_coefficient *
				    fe_face_values.shape_value (i,q_face_point) *
				    fe_face_values.shape_value (j,q_face_point) *
				    fe_face_values.JxW         (q_face_point);
				}
			    }
			  cell_rhs(i)+=
			    top_inbound_heat_flux_new*
			    time_step*theta_temperature*
			    fe_face_values.shape_value(i,q_face_point) *
			    fe_face_values.JxW(q_face_point)
			    +
			    top_inbound_heat_flux_old*
			    time_step*(1.-theta_temperature)*
			    fe_face_values.shape_value(i,q_face_point) *
			    fe_face_values.JxW(q_face_point);
			}
		    }
		}
	  }
	cell->get_dof_indices (local_dof_indices);

	for (unsigned int i=0; i<dofs_per_cell; ++i)
	  {
	    for (unsigned int j=0; j<dofs_per_cell; ++j)
	      {
		laplace_matrix_new.add (local_dof_indices[i],local_dof_indices[j],cell_laplace_matrix_new(i,j));
		laplace_matrix_old.add (local_dof_indices[i],local_dof_indices[j],cell_laplace_matrix_old(i,j));
		mass_matrix.add        (local_dof_indices[i],local_dof_indices[j],cell_mass_matrix(i,j)       );
	      }
	    system_rhs(local_dof_indices[i]) += cell_rhs(i);
	  }
      }

    Vector<double> tmp(solution.size ());
    /*
     * This is the section where the point source is included.
     * Notice that there are other ways to do this, but the
     * library has a function that is intended exactly for this
     * kind of problem. Check the documentation of
     * 'create_point_source_vector' in deal.ii
     * */
    if (parameters.point_source==true)
      {
	Point<dim> p(-1.*parameters.point_source_depth);
	VectorTools::create_point_source_vector(dof_handler,p,tmp);
	system_rhs.add(old_point_source_magnitude*(1.-theta_temperature)*time_step
		       +new_point_source_magnitude*(   theta_temperature)*time_step,tmp);// (W/m3)
      }
    //--------------------------------------------------------
    mass_matrix.vmult        ( tmp,old_solution);
    system_rhs.add           ( 1.0,tmp);
    laplace_matrix_old.vmult ( tmp,old_solution);
    system_rhs.add           (-(1.-theta_temperature) * time_step,tmp);

    system_matrix.copy_from (mass_matrix);
    system_matrix.add       (theta_temperature * time_step, laplace_matrix_new);

    hanging_node_constraints.condense (system_matrix);
    hanging_node_constraints.condense (system_rhs);

    if (parameters.fixed_at_bottom)
      {
	std::map<unsigned int,double> boundary_values;

	VectorTools::interpolate_boundary_values (dof_handler,
						  0,
						  ConstantFunction<dim>(parameters.bottom_fixed_value),
						  boundary_values);
	MatrixTools::apply_boundary_values (boundary_values,
					    system_matrix,
					    solution,
					    system_rhs);
      }
    if (parameters.boundary_condition_top.compare("first")==0)
      {
	std::map<unsigned int,double> boundary_values;
	VectorTools::interpolate_boundary_values (dof_handler,
						  1,
						  ConstantFunction<dim>(parameters.theta * new_surface_temperature +
									(1-parameters.theta) * old_surface_temperature),
						  boundary_values);
	MatrixTools::apply_boundary_values (boundary_values,
					    system_matrix,
					    solution,
					    system_rhs);
      }
  }

  template <int dim>
  void Heat_Pipe<dim>::solve_temperature()
  {
    SolverControl solver_control (solution.size(),
				  1e-8*system_rhs.l2_norm ());
    SolverCG<> cg (solver_control);

    PreconditionSSOR<> preconditioner;
    preconditioner.initialize (system_matrix, 1.2);

    cg.solve (system_matrix, solution, system_rhs,
	      preconditioner);

    hanging_node_constraints.distribute (solution);
  }

  template <int dim>
  void Heat_Pipe<dim>::fill_output_vectors()
  {
    /*
     * Extract and save temperatures at selected coordinates.
     **/
    if (dim==1)
      {
	std::vector<double> temp_vector;
	if (temperatures_at_points.size()==0)
	  {
	    for (unsigned int i=0; i<depths_coordinates.size(); i++)
	      temp_vector
		.push_back(VectorTools::point_value(dof_handler,solution,
						    Point<dim>(-1.*depths_coordinates[i][2])));
	  }
	else
	  {
	    for (unsigned int i=0; i<depths_coordinates.size(); i++)
	      temp_vector
		.push_back(VectorTools::point_value(dof_handler,solution,
						    Point<dim>(-1.*depths_coordinates[i][2])));
	  }
	//temp_vector.push_back(solution.l1_norm());
	temperatures_at_points.push_back(temp_vector);
	/*
	 * Save them to some file.
	 */
	output_file << timestep_number << "\t" << timestep_number*time_step;
	for (unsigned int i=0; i<temp_vector.size(); i++)
	  output_file << "\t" << std::setprecision(5) << temp_vector[i];

	output_file << "\t" << std::setprecision(5) << column_thermal_energy;
	output_file << std::endl;
      }
    else
      {
	std::cout << "Error in output_results function.\n"
		  << "Currently implement only for 1D\n";
	throw 1;
      }
  }

  template <int dim>
  void Heat_Pipe<dim>::output_results()
  {
    std::vector<double> ice_saturation_int;
    typename DoFHandler<dim>::active_cell_iterator
      cell = dof_handler.begin_active(),
      endc = dof_handler.end();
    for (; cell!=endc; ++cell)
      {
	double average_cell_temperature=
	  (   theta_temperature)*VectorTools::point_value(dof_handler,    solution,Point<dim>(cell->center()[0]))+
	  (1.-theta_temperature)*VectorTools::point_value(dof_handler,old_solution,Point<dim>(cell->center()[0]));

	double cell_thermal_conductivity          = -1.E10;
	double cell_total_volumetric_heat_capacity= -1.E10;
	double cell_thermal_energy                = -1.E10;
	double cell_ice_saturation                = -1.E10;
	material_data(cell->center()[0],average_cell_temperature,cell->diameter(),
		      cell_thermal_conductivity,cell_total_volumetric_heat_capacity,cell_thermal_energy,cell_ice_saturation);

	ice_saturation_int.push_back(cell_ice_saturation);
      }
    const Vector<double> ice_saturation(ice_saturation_int.begin(),ice_saturation_int.end());

    DataOut<dim> data_out;

    data_out.attach_dof_handler(dof_handler);
    data_out.add_data_vector(solution,"solution");
    data_out.add_data_vector(ice_saturation,"ice_saturation");
    data_out.build_patches();

    std::stringstream t;
    t << timestep_number;

    std::stringstream d;
    d << dim;

    std::string filename = parameters.output_directory + "/solution_"
      + d.str() + "d_time_"
      + t.str() + ".vtu";

    std::ofstream output (filename.c_str());
    data_out.write_vtu (output);
  }

  template <int dim>
  void Heat_Pipe<dim>::update_met_data ()
  {
    if (parameters.boundary_condition_top.compare("first")==0 ||
	parameters.boundary_condition_top.compare("third")==0)
      {
	/*
	 * Originally this function read a file with date (dd/mm/yyyy) and met data
	 * (air temperature, solar radiation, wind speed, etc) (hence the names).
	 * For the purpose of analysing a simplified composite region in 1D with fixed
	 * top boundary condition I'm assuming that we are reading a file with a single
	 * column corresponding to surface temperature at every time step. We can come
	 * back to more complex met data files later on
	 */
	if (met_data.size()==0)
	  {
	    DataTools data_tools;
	    std::vector<std::string> filenames;
	    filenames.push_back(parameters.top_fixed_value_file);
	    data_tools.read_data (filenames,
				  met_data);

	    std::cout << "\tAvailable surface data lines: " << met_data.size()
		      << std::endl << std::endl;

	    std::vector< std::pair<double,double> > table_temperature_surface;
	    std::vector< std::pair<double,double> > table_temperature_room;
	    for (unsigned int i=0; i<met_data.size(); i++)
	      {
		table_temperature_surface.push_back(std::make_pair(met_data[i][0],met_data[i][1]));
		table_temperature_room.push_back(std::make_pair(met_data[i][0],met_data[i][2]));
	      }

	    for (double t=table_temperature_surface[0].first;
		 t<table_temperature_surface[table_temperature_surface.size()-1].first; t+=time_step)
	      {
		std::vector<double> row_interpolated_temperature_surface;
		row_interpolated_temperature_surface.push_back(t);
		row_interpolated_temperature_surface.push_back(data_tools.interpolate_data(table_temperature_surface, t));

		interpolated_temperature_surface.push_back(row_interpolated_temperature_surface);

		std::vector<double> row_interpolated_temperature_room;
		row_interpolated_temperature_room.push_back(t);
		row_interpolated_temperature_room.push_back(data_tools.interpolate_data(table_temperature_room, t));

		interpolated_temperature_room.push_back(row_interpolated_temperature_room);
	      }

	    //			  for (unsigned int i=0; i<interpolated_temperature_surface.size(); i++)
	    //			  {
	    //				  std::cout << interpolated_temperature_surface[i][0] << "\t"
	    //						  << interpolated_temperature_surface[i][1] << "\t"
	    //						  << interpolated_temperature_room[i][1] << "\n";
	    //			  }

	  }

	// old_room_temperature    = interpolated_temperature_room[timestep_number-1][1];
	// new_room_temperature    = interpolated_temperature_room[timestep_number  ][1];
	// old_surface_temperature = interpolated_temperature_surface[timestep_number-1][1];
	// new_surface_temperature = interpolated_temperature_surface[timestep_number  ][1];
		  
	//		  old_room_temperature    = met_data[timestep_number-1][1];
	//		  new_room_temperature    = met_data[timestep_number  ][1];
	//		  old_surface_temperature = met_data[timestep_number-1][0];
	//		  new_surface_temperature = met_data[timestep_number  ][0];

	old_room_temperature    = 15.+10.*cos((2.*M_PI/86400)*((timestep_number-1)*time_step-54000));
	new_room_temperature    = 15.+10.*cos((2.*M_PI/86400)*(timestep_number*time_step-54000));
	old_surface_temperature = 15.+10.*cos((2.*M_PI/86400)*((timestep_number-1)*time_step-54000));
	new_surface_temperature = 15.+10.*cos((2.*M_PI/86400)*(timestep_number*time_step-54000));
      }


    if (parameters.point_source==true)
      {
	if (point_source_magnitudes.size()==0)
	  {
	    DataTools data_tools;
	    std::vector<std::string> filenames;
	    filenames.push_back(parameters.point_source_file);
	    data_tools.read_data (filenames,
				  point_source_magnitudes);

	    std::cout << "\n\tPoint source active at: " << parameters.point_source_depth
		      << "\n\tAvailable point source entries: "
		      << point_source_magnitudes.size()
		      << std::endl << std::endl;


	  }
	// old_point_source_magnitude =point_source_magnitudes[timestep_number-1][1];
	// new_point_source_magnitude =point_source_magnitudes[timestep_number  ][1];
	old_point_source_magnitude =-point_source_magnitudes[timestep_number-1][1]*sin((2.*M_PI/86400)*((timestep_number-1)*time_step-54000));
	new_point_source_magnitude =-point_source_magnitudes[timestep_number  ][1]*sin((2.*M_PI/86400)*((timestep_number  )*time_step-54000));
      }
  }

  template <int dim>
  void Heat_Pipe<dim>::initial_condition_temperature()
  {
    /*
      Here the vectors containing the name of the file with 
      the initial condition is defined
    */
    std::vector< std::string > filenames;
    filenames.push_back(parameters.initial_condition_file);
    /*
      And the vector containing the actual data (depth and
      temperature). Note that it is actually a vector of
      vector so, rather, a matrix.
      We use an external function to read the data and fill
      the initial condition matrix.
    */
    std::vector< std::vector<int> > dummy_matrix;

    std::vector< std::vector<double> > initial_condition;
    DataTools data_tools;
    data_tools.read_data(filenames,
			 initial_condition);
    /*
      The format of the matrix need to be changed from:

      ' std::vector< std::vector<double> > '
      to
      ' std::vector< std::pair<double,double> > '

      this is because the function that interpolates the 
      depths from the provided file takes this kind of
      format. Note that it is assumed that the matrix
      has two elements per row. If more are provided
      they will be ignored.
    */
    std::vector< std::pair<double,double> > initial_condition_table;
    for (unsigned int i=0; i<initial_condition.size(); i++)
      initial_condition_table
	.push_back(std::make_pair(initial_condition[i][0],
				  initial_condition[i][1]));
    /*
      Print number of lines available in the initial
      condition file and the actual data read.
      Currently it is expected a file with the following 
      format:
      Depth (m) \t Temperature (C)  <-- this line is not expected in the file

      0.0             T0
      d1              T1
      ...             ...
      dN              TN

      Note that the file starts with the temperature at x=0 m.
      If other depth is provided in the first line, the program
      will probalbly throw a not very informative error. Also,
      all depth values are positive.
    */
    std::cout << "Available initial condition entries: "
	      << initial_condition.size()    << std::endl
	      << "Initial condition: \n\tDepth\tTemperature (C)\n";
    for (unsigned int i=0; i<initial_condition.size(); i++)
      std::cout << "\t" << initial_condition[i][0] << "\t" << initial_condition[i][1] <<std::endl; 

    VectorTools::project (dof_handler,
			  hanging_node_constraints,
			  QGauss<dim>(2),
			  InitialValue<dim>(initial_condition_table),
			  old_solution);

    // VectorTools::project (dof_handler,
    // 			  hanging_node_constraints,
    // 			  QGauss<dim>(3),
    // 			  InitialValue<dim>(10.),
    // 			  old_solution);
    solution=old_solution;
  }

  template <int dim>
  void Heat_Pipe<dim>::run()
  {
    read_grid_temperature();
    setup_system_temperature();
    solution.reinit (dof_handler.n_dofs());
    old_solution.reinit (dof_handler.n_dofs());
    initial_condition_temperature();

    {
      double k=0.;
      double Cp=0.;
      double E=0.;
      double Si=0.;
      material_data(-parameters.material_0_depth-0.5*parameters.material_0_thickness,
		    25.,parameters.domain_size/pow(parameters.refinement_level,2.),
		    k,Cp,E,Si);
	    
      std::cout.setf( std::ios::fixed);
      std::cout.precision(3);
      std::cout << "\tPosition of material layers:\n"
		<< "\t\tLayer 1: from "
		<< parameters.material_0_depth << " to "
		<< parameters.material_0_depth+parameters.material_0_thickness << "\t"
		<< "k(@25C) :" << k << " W/mK\t"
		<< "Cp(@25C):" << Cp/1.E6 << " MJ/m3K\n";

      material_data(-parameters.material_1_depth-0.5*parameters.material_1_thickness,
		    25.,parameters.domain_size/pow(parameters.refinement_level,2.),
		    k,Cp,E,Si);
      std::cout << "\t\tLayer 2: from "
		<< parameters.material_1_depth << " to "
		<< parameters.material_1_depth+parameters.material_1_thickness << "\t"
		<< "k(@25C) :" << k << " W/mK\t"
		<< "Cp(@25C):" << Cp/1.E6 << " MJ/m3K\n";
	    
      material_data(-parameters.material_2_depth-0.5*parameters.material_2_thickness,
		    25.,parameters.domain_size/pow(parameters.refinement_level,2.),
		    k,Cp,E,Si);
      std::cout << "\t\tLayer 3: from "
		<< parameters.material_2_depth << " to "
		<< parameters.material_2_depth+parameters.material_2_thickness << "\t"
		<< "k(@25C) :" << k << " W/mK\t"
		<< "Cp(@25C):" << Cp/1.E6 << "MJ/m3K\n";

      material_data(-parameters.material_3_depth-0.5*parameters.material_3_thickness,
		    25.,parameters.domain_size/pow(parameters.refinement_level,2.),
		    k,Cp,E,Si);
      std::cout << "\t\tLayer 4: from "
		<< parameters.material_3_depth << " to "
		<< parameters.material_3_depth+parameters.material_3_thickness << "\t"
		<< "k(@25C) :" << k << " W/mK\t"
		<< "Cp(@25C):" << Cp/1.E6 << "MJ/m3K\n";

      material_data(-parameters.material_4_depth-0.5*parameters.material_4_thickness,
		    25.,parameters.domain_size/pow(parameters.refinement_level,2.),
		    k,Cp,E,Si);
      std::cout << "\t\tLayer 5: from "
		<< parameters.material_4_depth << " to "
		<< parameters.material_4_depth+parameters.material_4_thickness << "\t"
		<< "k(@25C) :" << k << " W/mK\t"
		<< "Cp(@25C):" << Cp/1.E6 << "MJ/m3K\n";
    }

    int output_count=0;
    for (timestep_number=1;//,time=time_step;
	 timestep_number<=timestep_number_max;//time<=time_max;
	 ++timestep_number)//time+=time_step
      {
	update_met_data();
	    
	int iteration=0;
	double total_error =1.E10;
	double solution_l1_norm_previous_iteration;
	double solution_l1_norm_current_iteration;
	do
	  {
	    //			  if (iteration!=0 && iteration%5==0)
	    //				  time_step/=2.;

	    assemble_system_temperature();

	    solution_l1_norm_previous_iteration=solution.l2_norm();
	    solve_temperature();
	    solution_l1_norm_current_iteration=solution.l2_norm();
			  
	    total_error=
	      1.-std::fabs(solution_l1_norm_previous_iteration/solution_l1_norm_current_iteration);

	    // std::cout << "\titeration " << iteration << "\tT@(0.0): "
	    //  << VectorTools::point_value(dof_handler,solution,Point<dim>(-1.*0.00)) << "\n";

	    iteration++;
	  }while (std::fabs(total_error)>5E-4);
		  
	time+=time_step;

	if (parameters.output_data_in_terminal==true)
	  std::cout << "Time step " << timestep_number << "\ttime: " << time/60 << " min\tDt: "
		    << time_step << " s\t#it: " << iteration
	    //  << "\tEnergy_surface: " << 100.*time << " J"
	    //  << "\tEnergy_column: " << column_thermal_energy << " J"
	    //  << "\tEnergy_error: "
	    //  << -100.*time-column_thermal_energy << "\t"
	    //  << "Ta:\t" << new_surface_temperature << "\t"
	    //  << VectorTools::point_value(dof_handler,solution,Point<dim>(-1.*0.00)) << "\t"
	    //  << VectorTools::point_value(dof_handler,solution,Point<dim>(-1.*0.20)) << "\t"
	    //  << VectorTools::point_value(dof_handler,solution,Point<dim>(-1.*0.40)) << "\t"
	    //  << VectorTools::point_value(dof_handler,solution,Point<dim>(-1.*0.60)) << "\t"
	    //  << VectorTools::point_value(dof_handler,solution,Point<dim>(-1.*0.80)) << "\t"
	    //  << VectorTools::point_value(dof_handler,solution,Point<dim>(-1.*1.00)) << "\t"
		    << "\n";

	if (parameters.output_frequency!=0 &&
	    time>output_count*parameters.output_frequency)
	  {
	    output_results();
	    output_count++;
	  }
	fill_output_vectors();

	// if (time==35.*3600.)
	//   for (unsigned int i=0; i<62; i++)
	//     std::cout << 0.01*i << "\t"
	// 		<< VectorTools::point_value(dof_handler,solution,Point<dim>(-1.*0.01*i)) << "\n";
		  
	old_solution=solution;

	//		  if (iteration<5)
	//			  time_step+=2.;
      }
    output_file.close();
    std::cout << "\t Job Done!!"
	      << std::endl;
  }
}

int main (int argc, char *argv[])
{
  // Material *sand1 = new Material("sand");
  // Material *sand2 = new Material(7,8,9);

  // PorousMaterial *soil1 = new PorousMaterial("land");
  // PorousMaterial *soil2 = new PorousMaterial(14,15,16);
  
  // std::cout << "Sand 1 thermal conductivity: "
  // 	    << sand1->thermal_conductivity() << "\n"
  // 	    << "Sand 2 thermal conductivity: "
  // 	    << sand2->thermal_conductivity() << "\n\n"
  // 	    << "Soil 1 thermal conductivity: "
  // 	    << soil1->thermal_conductivity() << "\n"
  //   	    << "Soil 2 thermal conductivity: "
  // 	    << soil2->thermal_conductivity() << "\n";

  try
    {
      using namespace TRL;
      using namespace dealii;
      {
	deallog.depth_console (0);

	Heat_Pipe<1> laplace_problem(argc,argv);

	laplace_problem.run();
      }
    }
  catch (std::exception &exc)
    {
      std::cerr << std::endl << std::endl
		<< "----------------------------------------------------"
		<< std::endl;
      std::cerr << "Exception on processing: " << std::endl
		<< exc.what() << std::endl
		<< "Aborting!" << std::endl
		<< "----------------------------------------------------"
		<< std::endl;

      return 1;
    }
  catch (...)
    {
      std::cerr << std::endl << std::endl
		<< "----------------------------------------------------"
		<< std::endl;
      std::cerr << "Unknown exception!" << std::endl
		<< "Aborting!" << std::endl
		<< "----------------------------------------------------"
		<< std::endl;
      return 1;
    }
  return 0;
}
