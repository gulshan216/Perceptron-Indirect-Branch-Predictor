# Perceptron-Indirect-Branch-Predictor
With the advent of object oriented programming languages like C++, Java, and C#, the problem of indirect branches is becoming increasingly significant. The most common example is that of the usage of Virtual functions whose targets are known only during run-time because they depend on the dynamic type of the object on which the function is called. Such indirect branch instructions impose a serious limit on the processor performance because predicting an indirect branch is more
difficult than predicting a conditional branch as it requires the prediction of the target address instead of the prediction of the branch direction.

This project aims at the implementation of a Virtual Program Counter(VPC) Predictor using a Hash Perceptron Conditional Branch predictor. The main idea of VPC prediction is that it treats a single indirect branch as multiple virtual conditional branches. The major advantage of this mechanism is that it uses the existing conditional branch prediction hardware to predict the targets of indirect branches, without requiring any program transformation or compiler support.

Please refer the [writeup](./Gulshan_Project1_Writeup.pdf) for more details 
