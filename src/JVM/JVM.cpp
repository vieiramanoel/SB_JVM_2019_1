#include <JVM/JVM.hpp>
#include <JVM/structures/ContextEntry.hpp>
#include <iostream>

JVM::JVM(ClassFile *cl) { class_loader = cl; }

ClassFields JVM::convertFieldIntoMap(std::vector<FieldInfoCte> fi) {
    ClassFields cf;
    for (auto f : fi) {
        if (f.descriptor.length() == 1) {
            int zero         = 0;
            auto zeroref     = reinterpret_cast<void *>(&zero);
            cf[f.name_index] = std::make_unique<ContextEntry>(
                "", TypeMap.at(f.descriptor), zeroref);
        } else {
            int dimension = 0;
            std::string type;
            for (auto desc = f.descriptor.begin(); desc < f.descriptor.end();
                 desc++) {
                if (*desc == '[') {
                    dimension++;
                    continue;
                }
                type.push_back(*desc);
                break;
            }
            std::shared_ptr<ContextEntry> array_operator = nullptr;
            if (dimension > 1) {
                array_operator =
                    std::make_shared<ContextEntry>(TypeMap.at(type), 1);
                std::shared_ptr<ContextEntry> init = array_operator;
                for (auto i = 0; i < dimension - 1; i++) {
                    auto newarray =
                        std::make_shared<ContextEntry>(TypeMap.at(type), 1);
                    array_operator->addToArray(0, newarray);
                    array_operator = std::move(array_operator->arrayRef[0]);
                }
                break;
                cf[f.name_index] = init;
            } else {
                cf[f.name_index] =
                    std::make_shared<ContextEntry>("", L, nullptr);
            }
        }
    }
    return cf;
}

void JVM::Run() {
    MethodInfoCte main = class_loader->getMainMethod();
    auto field_vector  = *class_loader->getFields();
    auto field_map     = convertFieldIntoMap(field_vector);

    if (main.attributes_count < 1) {
        throw std::out_of_range(
            "Method main must have only one code attribute, check .class file");
    }
    auto method_attribute_code = main.attributes[0];
    auto code_length           = method_attribute_code.code_length;
    if (!code_length) {
        std::cout << "No code to be executed" << std::endl;
        return;
    }
    ContextEntry main_context;
    std::vector<ContextEntry> context{main_context};
    stack_per_thread.push(StackFrame(context));

    auto code = method_attribute_code.code;
    executeByteCode(code, field_map);
}

void JVM::executeByteCode(std::vector<unsigned char> code, ClassFields cf) {
    auto context = stack_per_thread.top().lva;
    context.push_back(ContextEntry(&cf, nullptr));
    auto me = new MethodExecuter(code, class_loader->getCP());
    me->Exec(context);
}