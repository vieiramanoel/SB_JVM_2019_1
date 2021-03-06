#include <DotClassReader/Attributes.hpp>

///
/// Saves a reference of the .class file
///
Attributes::Attributes(std::ifstream *file) { this->file = file; }

///
/// Parses the .class file and fills the structures from the Attributes class.
///
void Attributes::seek() {
    attributes_count = getInfo(file, 2);
    for (int i = 0; i < attributes_count; i++) {
        auto attribute = AttributeClassFile{
            static_cast<unsigned short int>(getInfo(file, 2)),
            static_cast<unsigned int>(getInfo(file, 4)),
            static_cast<unsigned short int>(getInfo(file, 2)),
        };
        attr.push_back(attribute);
    }
}

///
/// Gets .class attributes
///
std::vector<AttributeClassFile> *Attributes::getClassAttributes() {
    return &attr;
}

///
/// Show Attributes by iterating
///
void Attributes::show() {
    std::cout << "--------------------------------------------" << std::endl;
    std::cout << "               Attributes" << std::endl;
    std::cout << "--------------------------------------------" << std::endl;
    for (auto attribute : attr) {
        std::cout << "  Generic Info: " << std::endl;
        std::cout << "    Attribute name index: cp_info #"
                  << attribute.attribute_name_index << "  " << attribute.name
                  << std::endl;
        std::cout << "     Attribute lenght: " << attribute.attribute_length
                  << std::endl;
        std::cout << "  Specific info:" << std::endl;
        std::cout << "    Source file name index: cp_info #"
                  << attribute.sourcefile_index << "  " << attribute.sourcefile
                  << std::endl;
    }
    std::cout << std::endl;
}

int Attributes::attrCount() { return attributes_count; }