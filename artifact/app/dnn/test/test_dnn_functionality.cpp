#include <vector>
#include <string>
#include <iostream>
using namespace std;

#define CATCH_CONFIG_MAIN
#include <artifact/test/catch.h>


#include <artifact/network/deep_network.h>
#include <artifact/network/network_creator.h>
#include <artifact/network/network_trainer.h>
#include <artifact/utils/matrix_io_txt.h>
#include <artifact/optimization/numerical_gradient.h>
#include <artifact/optimization/sgd_optimizer.h>
#include <artifact/optimization/gd_optimizer.h>
#include <artifact/optimization/mt_sgd_optimizer.h>

using namespace artifact::network;
using namespace artifact::utils;
using namespace artifact::optimization;

SCENARIO( "dnn can be created and operated correctly", "[dnn_prediction]" ) {

    GIVEN( "network created" ) {

        Eigen::initParallel();

        vector<int> layer_sizes = {25,500,500,1000,500,500,1};
        vector<string> layer_types = {"linear","linear","linear","linear","linear","logistic"};
        string net_loss = "mse";

        network_architecture arch;
        arch.layer_sizes = layer_sizes;
        arch.activator_types = layer_types;
        arch.loss = net_loss;

        random_network_creator creator;
        deep_network net = creator.create(arch);

        WHEN("network can be created")
        {
            THEN(" network architecture ")
            {
                REQUIRE(net.get_layer_num() == 6);

                for (int i = 0; i < layer_types.size(); i++)
                {
                    REQUIRE(net.get_layer(i).input_dim == net.get_layer(i).W.rows());
                    REQUIRE(net.get_layer(i).output_dim == net.get_layer(i).W.cols());
                    REQUIRE(net.get_layer(i).input_dim == layer_sizes[i]);
                    REQUIRE(net.get_layer(i).output_dim == layer_sizes[i+1]);
                }
            }
        }

        WHEN( "zero input is given" )
        {

            MatrixType input = MatrixType::Zero(100, 25);

            THEN("fist layer output zero matrix") {

                MatrixType output = net.get_layer(0).predict(input);
                REQUIRE(output.rows() == 100);
                REQUIRE(output.cols() == 500);
                REQUIRE(output.squaredNorm() < 1e-6);

            }
            THEN("the entire network output 0.5") {

                MatrixType output = net.predict(input);
                REQUIRE(output.rows() == 100);
                REQUIRE(output.cols() == 1);
                REQUIRE((output.array() - 0.5).matrix().norm() < 1e-6);

            }
            THEN("the mse error to vector of 0.5 is 0") {
                MatrixType y = MatrixType::Ones(100,1) * 0.5;

                NumericType loss = net.objective(input, &y);
                REQUIRE( loss < 1e-6);

            }
        }

    };
}


SCENARIO( "dnn can be optimized correctly", "[dnn_optimization]" ) {

    GIVEN( "network created" ) {
        Eigen::initParallel();

        vector<int> layer_sizes = {25,50,50,1};
        vector<string> layer_types = {"logistic","logistic","linear"};
        string net_loss = "mse";

        network_architecture arch;
        arch.layer_sizes = layer_sizes;
        arch.activator_types = layer_types;
        arch.loss = net_loss;

        random_network_creator creator;
        deep_network net = creator.create(arch);

        MatrixType X  = load_matrix_from_txt("train.X").transpose();
        MatrixType y = load_matrix_from_txt("train.y");

        WHEN("data is loaded")
        {
            THEN(" the loaded data is compatable with network")
            {
                REQUIRE(X.rows() == 1000);
                REQUIRE(X.cols() == 25);
                REQUIRE(X.rows() == y.size());
            }


        }

        WHEN( "parameter can be get and set ")
        {
            NumericType obj = net.objective(X,&y );

            MatrixType first_layer_W = net.get_layer(0).W;

            VectorType parameter = net.get_parameter();
            parameter(0) = 500;
            net.set_parameter(parameter);

            NumericType new_obj = net.objective(X, &y);
            MatrixType new_first_layer_W = net.get_layer(0).W;

            THEN(" the object should change after parameter is changed")
            {
                REQUIRE((new_first_layer_W - first_layer_W).norm() > 100);
                REQUIRE(abs(new_obj-obj) > 1e-3 );
            }

        }

        WHEN("gradient is calculated") {

            VectorType param = net.get_parameter();
            VectorType gradient(param.size());
            NumericType obj_val = 0.0;

            tie(obj_val, gradient) = net.gradient(X, &y);


            THEN(" it must be approximately equal to the numerical one")
            {
                if (typeid(NumericType) == typeid(double))
                {
                    VectorType n_gradient = numerical_gradient(net, param, X, &y);

                    REQUIRE((gradient - n_gradient).norm() < 1e-3);
                }


            }


        }

        WHEN("gd training is working ") {

            gd_optimizer optimizer_;
            optimizer_.learning_rate = 0.001;
            optimizer_.decay_rate = 0.9;
            optimizer_.max_epoches = 10;

            optimization_trainer trainer(optimizer_);

            NumericType obj_before_train = net.objective(X, &y );

            net = trainer.train(net,X,&y);

            NumericType obj_after_train = net.objective(X, &y );

            THEN(" the objective must decrease ")
            {

                REQUIRE(obj_after_train < obj_before_train);

            }


        }

        WHEN("mt_sgd training is working ") {

            mt_sgd_optimizer optimizer_;
            optimizer_.learning_rate = 0.001;
            optimizer_.decay_rate = 0.9;
            optimizer_.thread_num = 2;
            optimizer_.batch_per_thread = 20;
            optimizer_.max_epoches = 10;

            optimization_trainer trainer(optimizer_);

            NumericType obj_before_train = net.objective(X, &y );

            net = trainer.train(net,X,&y);

            NumericType obj_after_train = net.objective(X, &y );

            THEN(" the objective must decrease ")
            {

                REQUIRE(obj_after_train < obj_before_train);

            }


        }

    };
}