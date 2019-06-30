#include <JVM/structures/FieldMap.hpp>
#include <MethodExecuter/MethodExecuter.hpp>
#include <math.h>

MethodExecuter::MethodExecuter(ConstantPool *cp, ClassMethods *cm) {
    this->cp = cp;
    this->cm = cm;
}

ContextEntry
MethodExecuter::Exec(std::vector<unsigned char> bytecode,
                     std::vector<std::shared_ptr<ContextEntry>> ce) {
    sf = new StackFrame(ce);
    std::vector<int> args;
    int args_counter = 0;
    bool wide        = false;
    for (auto byte = bytecode.begin(); byte != bytecode.end(); byte++) {
        switch (*byte) {
        case 0x32: // aaload
        {
            auto index = sf->operand_stack.top()->context_value.i;
            sf->operand_stack.pop();
            if (sf->operand_stack.top()->isArray) {
                auto arrayRef = sf->operand_stack.top()->getArray();
                sf->operand_stack.pop();
                if (index >= arrayRef.size()) {
                    throw std::runtime_error("ArrayIndexOutOfBoundsException");
                }
                sf->operand_stack.push(arrayRef.at(index));
            } else {
                throw std::runtime_error(
                    "Stack operand is not an array reference");
            }
        } break;
        case 0x53: // aastore
        {
            auto value = sf->operand_stack.top();
            std::shared_ptr<ContextEntry> value_ref(
                std::shared_ptr<ContextEntry>(
                    new ContextEntry(std::move(*value))));
            sf->operand_stack.pop();
            auto index = sf->operand_stack.top()->context_value.i;
            sf->operand_stack.pop();
            if (sf->operand_stack.top()->isArray) {
                sf->operand_stack.top()->addToArray(index, value_ref);
            } else {
                throw std::runtime_error(
                    "Stack operand is not an array reference");
            }
        } break;
        case 0x01: // aconst_null
        {
            sf->operand_stack.push(
                std::shared_ptr<ContextEntry>(new ContextEntry()));
        } break;
        case 0x19: // aload
        {
            int index        = -1;
            const int index1 = *(byte + 1);
            if (wide) {
                const int index2 = *(byte + 2);
                index            = index1 << 8 + index2;
                byte++;
            } else {
                index = index1;
            }
            byte++;
            auto load = sf->lva.at(index);
            if (load->isReference()) {
                sf->operand_stack.push(load);
            } else {
                throw std::runtime_error("aload of non-reference object");
            }
        }
        case 0x2a: // aload_0
        case 0x2b: // aload_1
        case 0x2c: // aload_2
        case 0x2d: // aload_3
        {
            std::cout << "ENTROU NO ALOAD_0 com byte = " << *byte << std::endl;
            int index = *byte - 0x2a;
            auto load = sf->lva.at(index);
            if (load->isReference()) {
                sf->operand_stack.push(load);
            } else {
                throw std::runtime_error("aload of non-reference object");
            }
        } break;
        case 0xbd: // anewarray
        {
            int count = sf->operand_stack.top()->context_value.i;
            sf->operand_stack.pop();
            int index        = -1;
            const int index1 = *(byte + 1);
            if (wide) {
                const int index2 = *(byte + 2);
                index            = index1 << 8 + index2;
                byte++;
            } else {
                index = index1;
            }
            byte++;
            if (count < 0) {
                throw std::runtime_error("NegativeArraySizeException");
            }
            // do we need to save this into a heap? Idk
            sf->operand_stack.push(
                std::shared_ptr<ContextEntry>(new ContextEntry(L, count)));
        } break;
        case 0xb0: // areturn
        {
            auto retval = sf->operand_stack.top();
            while (!sf->operand_stack.empty()) {
                sf->operand_stack.pop();
            }
            if (!retval->isReference())
                throw std::runtime_error(
                    "areturn cannot return a non-reference value");
            return *retval;
        } break;
        case 0xbe: // arraylength
        {
            auto arrRef = sf->operand_stack.top();
            sf->operand_stack.pop();
            if (!arrRef->isReference()) {
                std::runtime_error(
                    "arraylength called over a non-reference object");
            }

            auto length     = arrRef->arrayLength();
            auto length_ptr = std::shared_ptr<ContextEntry>(
                new ContextEntry(std::move(length)));
            sf->operand_stack.push(length_ptr);
        } break;
        case 0x3a: // astore
        {
            int index        = -1;
            const int index1 = *(byte + 1);
            if (wide) {
                const int index2 = *(byte + 2);
                index            = index1 << 8 + index2;
                byte++;
            } else {
                index = index1;
            }
            byte++;
            auto objRef = sf->operand_stack.top();
            sf->operand_stack.pop();
            if (!objRef->isReference()) {
                std::runtime_error("astore called over a non-reference object");
            }
            // return address type??
            sf->lva[index] = objRef;
        } break;
        case 0x4b: // astore_0
        case 0x4c: // astore_1
        case 0x4d: // astore_2
        case 0x4e: // astore_3
        {
            unsigned int index = *byte - 0x4b;
            auto objRef        = sf->operand_stack.top();
            sf->operand_stack.pop();
            if (!objRef->isReference()) {
                std::runtime_error("astore called over a non-reference object");
            }
            sf->lva[index] = objRef;
        } break;
        case 0xbf: // athrow
        {
            // implement this;
        } break;
        case 0xbb: // new
        {
            if (byte + 2 >= bytecode.end()) {
                throw std::runtime_error(
                    "New instruction missing missing parameters, code ends "
                    "before two next args");
            }
            int classNameIndex = (static_cast<int>(*(byte + 1)) << 8) +
                                 static_cast<int>(*(byte + 2));
            std::string className = cp->getNameByIndex(classNameIndex);
            auto cevoid           = reinterpret_cast<void *>(&sf->lva);
            auto entry            = std::shared_ptr<ContextEntry>(
                new ContextEntry(className, L, cevoid));
            sf->operand_stack.push(entry);
            byte += 2;
        } break;
        case 0x59: // dup
        {
            auto top = sf->operand_stack.top();
            sf->operand_stack.push(top);
        } break;
        case 0x33: // baload
        case 0x34: // caload
        case 0x31: // daload
        case 0x30: // faload
        case 0x2e: // iaload
        case 0x2f: // laload
        case 0x35: // saload
        {
            auto index = sf->operand_stack.top()->context_value.i;
            sf->operand_stack.pop();
            auto arrayref = sf->operand_stack.top()->getArray();
            sf->operand_stack.pop();

            if (arrayref.size() >= index)
                throw std::runtime_error("ArrayIndexOutOfBoundsException");
            auto value = arrayref.at(index);
            sf->operand_stack.push(value);
        } break;
        case 0x54: // bastore
        case 0x55: // castore
        case 0x52: // dastore
        case 0x51: // fastore
        case 0x4f: // iastore
        case 0x50: // lastore
        case 0x56: // sastore
        {
            auto value = sf->operand_stack.top();
            sf->operand_stack.pop();
            auto index = sf->operand_stack.top()->context_value.i;
            sf->operand_stack.pop();
            auto arrayref = sf->operand_stack.top()->getArray();
            sf->operand_stack.pop();

            std::shared_ptr<ContextEntry> value_ref(
                std::shared_ptr<ContextEntry>(
                    new ContextEntry(std::move(*value))));
            arrayref[index] = value_ref;
            if (arrayref.size() >= index)
                throw std::runtime_error("ArrayIndexOutOfBoundsException");
            auto nvalue = arrayref.at(index);
            sf->operand_stack.push(nvalue);
        } break;
        case 0x10: // bipush
        {
            auto value = *(byte + 1);
            byte++;
            sf->operand_stack.push(std::shared_ptr<ContextEntry>(
                new ContextEntry("", B, reinterpret_cast<void *>(&value))));
        } break;
        case 0xc0: // checkcast
        {
            auto indexbyte1    = *(++byte);
            auto indexbyte2    = *(++byte);
            unsigned int index = (indexbyte1 << 8) + indexbyte2;
            auto objref        = sf->operand_stack.top();
            if (!objref->isNull) {
                sf->operand_stack.pop();
                if (objref->class_name == cp->getNameByIndex(index)) {
                    sf->operand_stack.push(objref);
                } else {
                    throw std::runtime_error("ClassCastException");
                }
            }
        } break;
        case 0x90: // d2f
        case 0x86: // i2f
        case 0x89: // l2f
        {
            auto value = sf->operand_stack.top();
            sf->operand_stack.pop();
            value->entry_type = F;
            sf->operand_stack.push(value);
        } break;
        case 0x8e: // d2i
        case 0x88: // l2i
        {
            auto value = sf->operand_stack.top();
            sf->operand_stack.pop();
            value->entry_type = I;
            sf->operand_stack.push(value);
        } break;
        case 0x8f: // d2l
        case 0x85: // i2l
        {
            auto value = sf->operand_stack.top();
            sf->operand_stack.pop();
            value->entry_type = L;
            sf->operand_stack.push(value);
        } break;
        case 0x63: // dadd
        case 0x62: // fadd
        case 0x60: // iadd
        case 0x61: // ladd
        {
            auto value1 = *sf->operand_stack.top();
            sf->operand_stack.pop();
            auto value2 = *sf->operand_stack.top();
            sf->operand_stack.pop();
            auto result = value1 + value2;
            sf->operand_stack.push(std::shared_ptr<ContextEntry>(
                new ContextEntry(std::move(result))));
        } break;
        case 0x98: // dcmp<op> dcmpg
        case 0x97: // dcmp<op> dcmpl
        {
            int i       = 1;
            auto value1 = sf->operand_stack.top();
            sf->operand_stack.pop();
            auto value2 = sf->operand_stack.top();
            sf->operand_stack.pop();
            auto entry = std::shared_ptr<ContextEntry>(
                new ContextEntry("", I, reinterpret_cast<void *>(&i)));
            if (value1->context_value.d > value2->context_value.d) {
                entry->context_value.i = 1;
                sf->operand_stack.push(entry);
            } else if (value1->context_value.d < value2->context_value.d) {
                entry->context_value.i = -1;
                sf->operand_stack.push(entry);
            } else if (value1->context_value.d == value2->context_value.d) {
                entry->context_value.i = 0;
                sf->operand_stack.push(entry);
            } else if (isnan(value1->context_value.d) ||
                       isnan(value2->context_value.d)) {
                if (*byte == 0x98) {
                    entry->context_value.i = 1;
                    sf->operand_stack.push(entry);
                } else if (*byte == 0x97) {
                    entry->context_value.i = -1;
                    sf->operand_stack.push(entry);
                }
            }
        } break;
        case 0xe: // dconst_<d> dconst_0
        case 0xf: // dconst_<d> dconst_1
        {
            char e = *byte - 0xe;
            sf->operand_stack.push(std::shared_ptr<ContextEntry>(
                new ContextEntry("", D, reinterpret_cast<void *>(&e))));

        } break;
        case 0x6f: // ddiv
        case 0x6e: // fdiv
        case 0x6d: // ldiv
        {
            auto value1 = *sf->operand_stack.top();
            sf->operand_stack.pop();
            auto value2 = *sf->operand_stack.top();
            sf->operand_stack.pop();
            if (value2.context_value.i == 0) {
                throw std::runtime_error("ArithmeticException");
            } else {
                auto result = value1 / value2;
                sf->operand_stack.push(std::shared_ptr<ContextEntry>(
                    new ContextEntry(std::move(result))));
            }
        } break;
        case 0x18: // dload
        case 0x16: // lload
        {
            int index        = -1;
            const int index1 = *(byte + 1);
            if (wide) {
                const int index2 = *(byte + 2);
                index            = index1 << 8 + index2;
                byte++;
            } else {
                index = index1;
            }
            byte++;
            auto value = sf->lva.at(index);
            sf->operand_stack.push(value);
        } break;
        case 0x26: // dload_0
        case 0x27: // dload_1
        case 0x28: // dload_2
        case 0x29: // dload_3
        {
            auto index = *(byte)-0x26;
            auto value = sf->lva.at(index);
            sf->operand_stack.push(value);
        } break;
        case 0x6b: // dmul
        case 0x6a: // fmul
        case 0x68: // imul
        case 0x69: // lmul
        {
            auto value1 = *sf->operand_stack.top();
            sf->operand_stack.pop();
            auto value2 = *sf->operand_stack.top();
            sf->operand_stack.pop();
            auto result = value1 * value2;
            sf->operand_stack.push(std::shared_ptr<ContextEntry>(
                new ContextEntry(std::move(result))));
        }
        case 0x77: // dneg
        {
            auto value = *sf->operand_stack.top();
            sf->operand_stack.pop();
            double d = -1;
            auto result =
                value * ContextEntry("", D, reinterpret_cast<void *>(&d));
            sf->operand_stack.push(std::shared_ptr<ContextEntry>(
                new ContextEntry(std::move(result))));
        } break;
        case 0x73: // drem
        {
            auto value1 = sf->operand_stack.top();
            sf->operand_stack.pop();
            auto value2 = sf->operand_stack.top();
            sf->operand_stack.pop();

            auto result =
                fmod(value1->context_value.d, value2->context_value.d);

            sf->operand_stack.push(std::shared_ptr<ContextEntry>(
                new ContextEntry("", D, reinterpret_cast<void *>(&result))));
        } break;
        case 0xaf: // dreturn
        case 0xae: // freturn

        {
            auto retval = *sf->operand_stack.top();
            while (!sf->operand_stack.empty()) {
                sf->operand_stack.pop();
            }
            return retval;
        } break;
        case 0x39: // dstore
        case 0x37: // lstore

        {
            int index        = -1;
            const int index1 = *(byte + 1);
            if (wide) {
                const int index2 = *(byte + 2);
                index            = index1 << 8 + index2;
                byte++;
            } else {
                index = index1;
            }
            auto value = sf->operand_stack.top();
            sf->operand_stack.pop();
            sf->lva[index]     = value;
            sf->lva[index + 1] = value;
        } break;
        case 0x47: // dstore_0
        case 0x48: // dstore_1
        case 0x49: // dstore_2
        case 0x4a: // dstore_3
        {
            auto index = *(byte)-0x47;
            auto value = sf->operand_stack.top();
            sf->operand_stack.pop();

            if (index == sf->lva.size()) {
                sf->lva.push_back(value);
            } else if (index > sf->lva.size()) {
                throw std::runtime_error("ArrayIndexOutOfBoundsException");
            } else {
                sf->lva.erase(sf->lva.begin() + index);
                sf->lva.insert(sf->lva.begin() + index, value);
            }
            if ((index + 1) == sf->lva.size()) {
                sf->lva.push_back(value);
            } else if ((index + 1) > sf->lva.size()) {
                throw std::runtime_error("ArrayIndexOutOfBoundsException");
            } else {
                sf->lva.erase(sf->lva.begin() + index);
                sf->lva.insert(sf->lva.begin() + index, value);
            }

        } break;

        case 0x66: // fsub
        case 0x67: // dsub
        case 0x64: // isub
        case 0x65: // lsub
        {
            auto value1 = *sf->operand_stack.top();
            sf->operand_stack.pop();
            auto value2 = *sf->operand_stack.top();
            sf->operand_stack.pop();
            ContextEntry result = value1 - value2;
            sf->operand_stack.push(std::shared_ptr<ContextEntry>(
                new ContextEntry(std::move(result))));
        } break;
        case 0x5a: // dup_x1
        {
            auto value1 = sf->operand_stack.top();
            sf->operand_stack.pop();
            auto value2 = sf->operand_stack.top();
            sf->operand_stack.pop();
            sf->operand_stack.push(value1);
            sf->operand_stack.push(value2);
            sf->operand_stack.push(value1);
        } break;
        case 0x5b: // dup_x2
        {
            auto value1 = sf->operand_stack.top();
            sf->operand_stack.pop();
            auto value2 = sf->operand_stack.top();
            sf->operand_stack.pop();
            if (value2->entry_type == D) {
                sf->operand_stack.push(value1);
                sf->operand_stack.push(value2);
                sf->operand_stack.push(value1);
            } else {
                auto value3 = sf->operand_stack.top();
                sf->operand_stack.pop();
                sf->operand_stack.push(value1);
                sf->operand_stack.push(value3);
                sf->operand_stack.push(value2);
                sf->operand_stack.push(value1);
            }
        } break;

        case 0x5c: // dup2
        {
            auto value1 = sf->operand_stack.top();
            sf->operand_stack.pop();
            if (category(value1->entry_type) == 2) {
                auto value = value1;
                sf->operand_stack.push(value);
                sf->operand_stack.push(value);
            } else {
                auto value2 = sf->operand_stack.top();
                sf->operand_stack.pop();
                sf->operand_stack.push(value1);
                sf->operand_stack.push(value2);
            }
        } break;
        case 0x5d: // dup2_x1
        {
            auto value1 = sf->operand_stack.top();
            sf->operand_stack.pop();
            if (category(value1->entry_type) == 2) {
                auto value2 = sf->operand_stack.top();
                sf->operand_stack.pop();
                sf->operand_stack.push(value1);
                sf->operand_stack.push(value2);
                sf->operand_stack.push(value1);
            } else {
                auto value2 = sf->operand_stack.top();
                sf->operand_stack.pop();
                auto value3 = sf->operand_stack.top();
                sf->operand_stack.pop();
                sf->operand_stack.push(value2);
                sf->operand_stack.push(value1);
                sf->operand_stack.push(value3);
                sf->operand_stack.push(value2);
                sf->operand_stack.push(value1);
            }
        } break;
        case 0x5e: // dup2_x2
        {
            auto value1 = sf->operand_stack.top();
            sf->operand_stack.pop();
            auto value2 = sf->operand_stack.top();
            sf->operand_stack.pop();
            if (category(value1->entry_type) == 2 &&
                category(value2->entry_type) ==
                    2) { // Form 4: value2, value1 -> value1, value2, value1
                sf->operand_stack.push(value1);
                sf->operand_stack.push(value2);
                sf->operand_stack.push(value1);
            } else if (category(value1->entry_type) == 1 &&
                       category(value2->entry_type) == 1) {
                auto value3 = sf->operand_stack.top();
                sf->operand_stack.pop();
                if (category(value3->entry_type) ==
                    2) { // Form 3: value3, value2, value1 -> value2, value1,
                         // value3, value2, value1
                    sf->operand_stack.push(value2);
                    sf->operand_stack.push(value1);
                    sf->operand_stack.push(value3);
                    sf->operand_stack.push(value2);
                    sf->operand_stack.push(value1);
                } else { // Form 1
                    auto value4 = sf->operand_stack.top();
                    sf->operand_stack.pop();
                    if (category(value4->entry_type) == 1) {
                        sf->operand_stack.push(value2);
                        sf->operand_stack.push(value1);
                        sf->operand_stack.push(value4);
                        sf->operand_stack.push(value3);
                        sf->operand_stack.push(value2);
                        sf->operand_stack.push(value1);
                    }
                }
            } else if (category(value1->entry_type) == 2 &&
                       category(value2->entry_type) == 1) {
                auto value3 = sf->operand_stack.top();
                sf->operand_stack.pop();
                if (value3->entry_type ==
                    1) { // Form 2: value3, value2, value1 →
                         // value1, value3, value2, value1
                    sf->operand_stack.push(value1);
                    sf->operand_stack.push(value3);
                    sf->operand_stack.push(value2);
                    sf->operand_stack.push(value1);
                }
            }
        } break;

        case 0x8d: // f2d
        case 0x87: // i2d
        case 0x8a: // l2d
        {
            auto value = sf->operand_stack.top();
            sf->operand_stack.pop();
            value->entry_type = D;
            sf->operand_stack.push(value);

        } break;
        case 0x8b: // f2i
        {
            auto value = sf->operand_stack.top();
            sf->operand_stack.pop();
            value->entry_type = I;
            sf->operand_stack.push(value);

        } break;
        case 0x8c: // f2l
        {
            auto value = sf->operand_stack.top();
            sf->operand_stack.pop();
            value->entry_type = L;
            sf->operand_stack.push(value);

        } break;
        case 0x96: // fcmpg
        case 0x95: // fcmppl
        {
            int i       = 1;
            auto value1 = sf->operand_stack.top();
            sf->operand_stack.pop();
            auto value2 = sf->operand_stack.top();
            sf->operand_stack.pop();
            auto entry = std::shared_ptr<ContextEntry>(
                new ContextEntry("", I, reinterpret_cast<void *>(&i)));
            if (value1->context_value.d > value2->context_value.d) {
                entry->context_value.i = 1;
                sf->operand_stack.push(entry);
            } else if (value1->context_value.d < value2->context_value.d) {
                entry->context_value.i = -1;
                sf->operand_stack.push(entry);
            } else if (value1->context_value.d == value2->context_value.d) {
                entry->context_value.i = 0;
                sf->operand_stack.push(entry);
            } else if (isnan(value1->context_value.d) ||
                       isnan(value2->context_value.d)) {
                if (*byte == 0x96) {
                    entry->context_value.i = 1;
                    sf->operand_stack.push(entry);
                } else if (*byte == 0x95) {
                    entry->context_value.i = -1;
                    sf->operand_stack.push(entry);
                }
            }
        } break;
        case 0xb: // fconst_0
        case 0xc: // fconst_1
        case 0xd: // fconst_2
        {
            char e     = *byte - 0xb;
            auto entry = std::shared_ptr<ContextEntry>(
                new ContextEntry("", F, reinterpret_cast<void *>(&e)));
            sf->operand_stack.push(entry);
        } break;
        case 0x17: // fload
        case 0x15: // iload
        {
            int index        = -1;
            const int index1 = *(byte + 1);
            if (wide) {
                const int index2 = *(byte + 2);
                index            = index1 << 8 + index2;
                byte++;
            } else {
                index = index1;
            }
            byte++;
            auto value = sf->lva.at(index);
            sf->operand_stack.push(value);

        } break;
        case 0x22: // fload_0
        case 0x23: // fload_1
        case 0x24: // fload_2
        case 0x25: // fload_3
        {
            auto index = *(byte)-0x22;
            auto value = sf->lva.at(index);
            sf->operand_stack.push(value);
        } break;
        case 0x76: // fneg
        {
            auto value = *sf->operand_stack.top();
            sf->operand_stack.pop();
            float f = -1;
            auto result =
                value * ContextEntry("", F, reinterpret_cast<void *>(&f));
            sf->operand_stack.push(std::shared_ptr<ContextEntry>(
                new ContextEntry(std::move(result))));

        } break;
        case 0x72: // frem
        {
            auto value1 = sf->operand_stack.top();
            sf->operand_stack.pop();
            auto value2 = sf->operand_stack.top();
            sf->operand_stack.pop();

            auto result =
                fmod(value1->context_value.f, value2->context_value.f);

            sf->operand_stack.push(std::shared_ptr<ContextEntry>(
                new ContextEntry("", F, reinterpret_cast<void *>(&result))));
        } break;
        case 0x38: // fstore
        case 0x36: // istore
        {
            int index        = -1;
            const int index1 = *(byte + 1);
            if (wide) {
                const int index2 = *(byte + 2);
                index            = index1 << 8 + index2;
                byte++;
            } else {
                index = index1;
            }
            auto value = sf->operand_stack.top();
            sf->operand_stack.pop();
            sf->lva[index] = value;
        } break;
        case 0x43: // fstore_0
        case 0x44: // fstore_1
        case 0x45: // fstore_2
        case 0x46: // fstore_3
        {
            auto index = *(byte)-0x43;
            auto value = sf->operand_stack.top();
            sf->operand_stack.pop();
            sf->lva[index] = value;
        } break;
        case 0xb4: // getfield
        {
            auto indexbyte1 = *(++byte);
            auto indexbyte2 = *(++byte);
            int index       = (indexbyte1 << 8) + indexbyte2;
            auto objref     = sf->operand_stack.top();
            sf->operand_stack.pop();
            if (objref->isNull)
                throw std::runtime_error("NullPointerException");
            auto value = objref->cf->at(index);
            sf->operand_stack.push(value);
        } break;
        case 0xb2: // getstatic
        {
            auto indexbyte1 = *(++byte);
            auto indexbyte2 = *(++byte);
            auto index      = (indexbyte1 << 8) | indexbyte2;
        } break;
        case 0xa7: // goto
        {
            auto branchbyte1 = *(++byte);
            auto branchbyte2 = *(++byte);

            auto offset = (branchbyte1 << 8) | branchbyte2;

            byte += offset;
        } break;
        case 0xc8: // goto_w
        {
            auto branchbyte1 = *(++byte);
            auto branchbyte2 = *(++byte);
            auto branchbyte3 = *(++byte);
            auto branchbyte4 = *(++byte);

            auto offset = (branchbyte1 << 24) | (branchbyte2 << 16) |
                          (branchbyte3 << 8) | branchbyte4;

            byte += offset;
        } break;
        case 0x91: // i2b
        case 0x92: // i2c
        {
            auto value = sf->operand_stack.top();
            sf->operand_stack.pop();
            value->entry_type = C;
            sf->operand_stack.push(value);
        } break;
        case 0x93: // i2s
        {
            auto value = sf->operand_stack.top();
            sf->operand_stack.pop();
            value->entry_type = S;
            sf->operand_stack.push(value);
        } break;
        case 0x7e: // iand
        case 0x7f: // land
        {
            auto value1 = *sf->operand_stack.top();
            sf->operand_stack.pop();
            auto value2 = *sf->operand_stack.top();
            sf->operand_stack.pop();
            auto result = value1 & value2;
            sf->operand_stack.push(std::shared_ptr<ContextEntry>(
                new ContextEntry(std::move(result))));
        } break;
        case 0x2: // iconst_m1
        case 0x3: // iconst_0
        case 0x4: // iconst_1
        case 0x5: // iconst_2
        case 0x6: // iconst_3
        case 0x7: // iconst_4
        case 0x8: // iconst_5
        {
            char e = *byte - 0x3;
            sf->operand_stack.push(std::shared_ptr<ContextEntry>(
                new ContextEntry("", I, reinterpret_cast<void *>(&e))));
        } break;
        case 0x6c: // idiv
        {
            auto value1 = *sf->operand_stack.top();
            sf->operand_stack.pop();
            auto value2 = *sf->operand_stack.top();
            sf->operand_stack.pop();
            if (value2.context_value.i == 0) {
                throw std::runtime_error("ArithmeticException");
            } else {
                auto value3 = value1 / value2;
                sf->operand_stack.push(std::shared_ptr<ContextEntry>(
                    new ContextEntry(std::move(value3))));
            }
        } break;
        case 0xa5: // if_acmpeq
        case 0xa6: // if_acmpne
        {
            auto index  = *(byte)-0xa5;
            auto value1 = *sf->operand_stack.top();
            sf->operand_stack.pop();
            auto value2 = *sf->operand_stack.top();
            sf->operand_stack.pop();

            auto branchbyte1 = *(++byte);
            auto branchbyte2 = *(++byte);
            if (index == 0) {
                if (value1.l == value2.l) {
                    auto offset = (branchbyte1 << 8) | branchbyte2;
                    byte += offset;
                }
            } else if (index == 1) {
                if (value1.l != value2.l) {
                    auto offset = (branchbyte1 << 8) | branchbyte2;
                    byte += offset;
                }
            }
        } break;
        case 0x9f: // if_icmpeq
        case 0xa0: // if_icmpne
        case 0xa1: // if_icmplt
        case 0xa2: // if_icmpge
        case 0xa3: // if_icmpgt
        case 0xa4: // if_icmple
        {
            auto index  = *(byte)-0x9f;
            auto value1 = sf->operand_stack.top();
            sf->operand_stack.pop();
            auto value2 = sf->operand_stack.top();
            sf->operand_stack.pop();

            auto branchbyte1 = *(++byte);
            auto branchbyte2 = *(++byte);

            if (index == 0) {
                if (value1->context_value.i == value2->context_value.i) {
                    auto offset = (branchbyte1 << 8) | branchbyte2;
                    byte += offset;
                }
            } else if (index == 1) {
                if (value1->context_value.i != value2->context_value.i) {
                    auto offset = (branchbyte1 << 8) | branchbyte2;
                    byte += offset;
                } else if (index == 2) {
                    if (value1->context_value.i < value2->context_value.i) {
                        auto offset = (branchbyte1 << 8) | branchbyte2;
                        byte += offset;
                    } else if (index == 3) {
                        if (value1->context_value.i <=
                            value2->context_value.i) {
                            auto offset = (branchbyte1 << 8) | branchbyte2;
                            byte += offset;
                        } else if (index == 4) {
                            if (value1->context_value.i >
                                value2->context_value.i) {
                                auto offset = (branchbyte1 << 8) | branchbyte2;
                                byte += offset;
                            } else if (index == 5) {
                                if (value1->context_value.i >=
                                    value2->context_value.i) {
                                    auto offset =
                                        (branchbyte1 << 8) | branchbyte2;
                                    byte += offset;
                                }
                            }
                        }
                    }
                }
            }
        } break;
        case 0x99: // ifeq
        case 0x9a: // ifne
        case 0x9b: // iflt
        case 0x9c: // ifge
        case 0x9d: // ifgt
        case 0x9e: // ifle
        {
            auto value = sf->operand_stack.top();
            sf->operand_stack.pop();
            int n = *(byte)-0x99;
            if (n == 0) {
                if (!value->context_value.i) {
                    auto branchbyte1 = *(++byte);
                    auto branchbyte2 = *(++byte);
                    auto offset      = (branchbyte1 << 8) | branchbyte2;
                    byte             = byte + offset;
                } else if (n == 1) {
                    if (value->context_value.i) {
                        auto branchbyte1 = *(++byte);
                        auto branchbyte2 = *(++byte);
                        auto offset      = (branchbyte1 << 8) | branchbyte2;
                        byte             = byte + offset;
                    }
                } else if (n == 2) {
                    if (value->context_value.i < 0) {
                        auto branchbyte1 = *(++byte);
                        auto branchbyte2 = *(++byte);
                        auto offset      = (branchbyte1 << 8) | branchbyte2;
                        byte             = byte + offset;
                    }
                } else if (n == 3) {
                    if (value->context_value.i >= 0) {
                        auto branchbyte1 = *(++byte);
                        auto branchbyte2 = *(++byte);
                        auto offset      = (branchbyte1 << 8) | branchbyte2;
                        byte             = byte + offset;
                    }
                } else if (n == 4) {
                    if (value->context_value.i > 0) {
                        auto branchbyte1 = *(++byte);
                        auto branchbyte2 = *(++byte);
                        auto offset      = (branchbyte1 << 8) | branchbyte2;
                        byte             = byte + offset;
                    }
                } else {
                    if (value->context_value.i <= 0) {
                        auto branchbyte1 = *(++byte);
                        auto branchbyte2 = *(++byte);
                        auto offset      = (branchbyte1 << 8) | branchbyte2;
                        byte             = byte + offset;
                    }
                }
            }
        } break;
        case 0xc6: // ifnull
        case 0xc7: // ifnonnull
        {
            auto index = *(byte)-0xc6;
            auto value = *sf->operand_stack.top();
            sf->operand_stack.pop();
            if (index == 0) {
                if (value.isNull) {
                    auto branchbyte1 = *(++byte);
                    auto branchbyte2 = *(++byte);
                    auto offset      = (branchbyte1 << 8) | branchbyte2;
                }
            } else {
                if (!value.isNull) {
                    auto branchbyte1 = *(++byte);
                    auto branchbyte2 = *(++byte);
                    auto offset      = (branchbyte1 << 8) | branchbyte2;
                }
            }
        } break;
        case 0x84: // iinc
        {
            auto index    = *(++byte);
            char constant = *(++byte);
            sf->lva[index]->context_value.i += constant;

        } break;
        case 0xc1: // instanceof
        {
            // I'm not sure about if this will work or not;
            auto indexbyte1    = *(++byte);
            auto indexbyte2    = *(++byte);
            unsigned int index = (indexbyte1 << 8) + indexbyte2;
            auto objref        = sf->operand_stack.top();
            int zero           = 0;
            int one            = 1;
            sf->operand_stack.pop();
            if (objref->isNull) {
                // push 0 into the stack
                sf->operand_stack.push(std::shared_ptr<ContextEntry>(
                    new ContextEntry("", I, reinterpret_cast<void *>(&zero))));
            } else {
                if (objref->class_name == cp->getNameByIndex(index)) {
                    sf->operand_stack.push(
                        std::shared_ptr<ContextEntry>(new ContextEntry(
                            "", I, reinterpret_cast<void *>(&one))));
                } else {
                    sf->operand_stack.push(
                        std::shared_ptr<ContextEntry>(new ContextEntry(
                            "", I, reinterpret_cast<void *>(&zero))));
                }
            }
        } break;
        case 0x1a: // iload_0
        case 0x1b: // iload_1
        case 0x1c: // iload_2
        case 0x1d: // iload_3
        {
            auto index = *(byte)-0x1a;
            auto value = sf->lva.at(index);
            sf->operand_stack.push(value);

        } break;

        case 0x74: // imul
        {
            auto value = sf->operand_stack.top();
            sf->operand_stack.pop();
            int i = -1;
            auto result =
                *value * ContextEntry("", I, reinterpret_cast<void *>(&i));
            sf->operand_stack.push(std::shared_ptr<ContextEntry>(
                new ContextEntry(std::move(result))));
        } break;
        case 0xba: // invokedynamic
            break;
        case 0xb9: // invokeinterface
            break;
        case 0xb8: // invokestatic
            break;
        case 0xb6: // invokevirtual
        {
            int indexbyte1 = *(++byte);
            int indexbyte2 = *(++byte);
            auto index     = (indexbyte1 << 8) | indexbyte2;

            auto value = cp->getMethodNameIndex(index);

            if (value == -2) { // means cp(index) = java/io/PrintStream
                auto name_and_type = cp->getNameAndTypeByIndex(index);
                auto found         = name_and_type.find("println");
                if (found != std::string::npos) {
                    auto args =
                        std::string(name_and_type.begin() +
                                        name_and_type.find_first_of('(') + 1,
                                    name_and_type.begin() +
                                        name_and_type.find_first_of(')'));
                    std::vector<std::shared_ptr<ContextEntry>> prints(
                        args.size());
                    for (auto &print : prints) {
                        print = sf->operand_stack.top();
                        sf->operand_stack.pop();
                    }
                    for (auto it = prints.end() - 1; it >= prints.begin();
                         it--) {
                        it->get()->PrintValue();
                        std::cout << std::endl;
                    }
                }
            }

        }

        break;

        case 0x80: // ior
        case 0x81: // lor
        {
            auto value1 = *sf->operand_stack.top();
            sf->operand_stack.pop();
            auto value2 = *sf->operand_stack.top();
            sf->operand_stack.pop();
            auto result = value1 || value2;
            sf->operand_stack.push(std::shared_ptr<ContextEntry>(
                new ContextEntry(std::move(result))));

        } break;
        case 0x70: // irem
        {
            auto value1 = *sf->operand_stack.top();
            sf->operand_stack.pop();
            auto value2 = *sf->operand_stack.top();
            sf->operand_stack.pop();

            auto result = value1.context_value.i % value2.context_value.i;

            if (result == 0) {
                throw std::runtime_error("ArithmeticException");

                sf->operand_stack.push(
                    std::shared_ptr<ContextEntry>(new ContextEntry(
                        "", I, reinterpret_cast<void *>(&result))));
            }
        } break;
        case 0xac: // ireturn

        case 0x78: // ishl
        {
            auto value1 = *sf->operand_stack.top();
            sf->operand_stack.pop();
            int value2 = sf->operand_stack.top()->context_value.i & 0x1f;
            sf->operand_stack.pop();

            auto result = value1.context_value.i << value2;
            sf->operand_stack.push(std::shared_ptr<ContextEntry>(
                new ContextEntry("", I, reinterpret_cast<void *>(&result))));
        } break;
        case 0x7a: // ishr
        {
            auto value1 = *sf->operand_stack.top();
            sf->operand_stack.pop();
            int value2 = sf->operand_stack.top()->context_value.i & 0x1f;
            sf->operand_stack.pop();
            ContextEntry("", I, static_cast<void *>(&value1));
            auto result = value1.context_value.i >> value2;
            sf->operand_stack.push(std::shared_ptr<ContextEntry>(
                new ContextEntry("", I, reinterpret_cast<void *>(&result))));
        } break;
        case 0xb7: // invokespecial
        {

            auto indexbyte1    = *(++byte);
            auto indexbyte2    = *(++byte);
            unsigned int index = (indexbyte1 << 8) + indexbyte2;
            auto cm_index      = cp->getMethodNameIndex(index);
            if (cm_index != -1) {
                std::vector<unsigned char> code(
                    cm->at(cm_index).attributes[0].code);
                Exec(code, sf->lva);
            }
        } break;
        case 0x3b: // istore_0
        case 0x3c: // istore_1
        case 0x3d: // istore_2
        case 0x3e: // istore_3
        {
            auto index = *(byte)-0x3b;
            auto value = sf->operand_stack.top();
            sf->operand_stack.pop();
            sf->lva[index] = value;
        } break;
        case 0x7c: // iushr
        {
            auto value1 = sf->operand_stack.top();
            sf->operand_stack.pop();
            auto value2 = sf->operand_stack.top()->context_value.i & 0x1f;
            sf->operand_stack.pop();

            auto result = value1->context_value.i >> value2;
            sf->operand_stack.push(std::shared_ptr<ContextEntry>(
                new ContextEntry("", I, reinterpret_cast<void *>(&result))));
        } break;
        case 0x82: // ixor
        case 0x83: // lxor
        {
            auto value1 = *sf->operand_stack.top();
            sf->operand_stack.pop();
            auto value2 = *sf->operand_stack.top();
            sf->operand_stack.pop();
            auto result = value1 ^ value2;
            sf->operand_stack.push(std::shared_ptr<ContextEntry>(
                new ContextEntry(std::move(result))));
        } break;
        case 0xa8: // jsr
        {
            // auto branchbyte1 = *(++byte);
            // auto branchbyte2 = *(++byte);

            // auto offset = (branchbyte1 << 8) | branchbyte2;

            // byte += offset;

        } break;
        case 0xc9: // jsr_w
        {
            // auto branchbyte1 = *(++byte);
            // auto branchbyte2 = *(++byte);
            // auto branchbyte3 = *(++byte);
            // auto branchbyte4 = *(++byte);

            // auto offset = (branchbyte1 << 24) | (branchbyte2 << 16) |
            //               (branchbyte3 << 8) | branchbyte4;

            // byte += offset;
        } break;
        case 0x94: // lcmp
        {
            int i       = 1;
            auto value1 = sf->operand_stack.top();
            sf->operand_stack.pop();
            auto value2 = sf->operand_stack.top();
            sf->operand_stack.pop();
            auto entry = ContextEntry("", I, reinterpret_cast<void *>(&i));
            if (value1->context_value.j > value2->context_value.j) {
                entry.context_value.i = 1;
                sf->operand_stack.push(std::shared_ptr<ContextEntry>(
                    new ContextEntry("", I, reinterpret_cast<void *>(&i))));
            } else if (value1->context_value.j < value2->context_value.j) {
                entry.context_value.i = -1;
                sf->operand_stack.push(std::shared_ptr<ContextEntry>(
                    new ContextEntry("", I, reinterpret_cast<void *>(&i))));
            } else if (value1->context_value.j == value2->context_value.j) {
                entry.context_value.i = 0;
                sf->operand_stack.push(std::shared_ptr<ContextEntry>(
                    new ContextEntry("", I, reinterpret_cast<void *>(&i))));
            }
        } break;
        case 0x9: // lconst_0
        case 0xa: // lconst_1
        {
            char e = *byte - 0x9;
            sf->operand_stack.push(std::shared_ptr<ContextEntry>(
                new ContextEntry("", I, reinterpret_cast<void *>(&e))));

        } break;
        case 0x12: // ldc
        case 0x13: // ldc_w
            break;
        case 0x14: // ldc2_w
        {
            auto index1   = *(++byte);
            auto index2   = *(++byte);
            auto index    = (index1 << 8) | index2;
            DoubleLong dl = cp->getNumberByIndex(index);
            auto cte      = std::shared_ptr<ContextEntry>(
                new ContextEntry("", dl.t, reinterpret_cast<void *>(&dl.val)));
            sf->operand_stack.push(cte);
        } break;
        case 0x1e: // lload_0
        case 0x1f: // lload_1
        case 0x20: // lload_2
        case 0x21: // lload_3
        {
            auto index = *(byte)-0x1e;
            auto value = sf->lva.at(index);
            sf->operand_stack.push(value);
            sf->operand_stack.push(value);

        } break;
        case 0x75: // lneg
        {
            auto value = *sf->operand_stack.top();
            sf->operand_stack.pop();
            double j = -1;
            auto result =
                value * ContextEntry("", J, reinterpret_cast<void *>(&j));
            sf->operand_stack.push(std::shared_ptr<ContextEntry>(
                new ContextEntry(std::move(result))));
        } break;
        case 0xab: // lookupswitch
        case 0x71: // irem
        {
            auto value1 = *sf->operand_stack.top();
            sf->operand_stack.pop();
            auto value2 = *sf->operand_stack.top();
            sf->operand_stack.pop();

            auto result = value1.context_value.i % value2.context_value.i;

            if (result == 0) {
                throw std::runtime_error("ArithmeticException");

                sf->operand_stack.push(
                    std::shared_ptr<ContextEntry>(new ContextEntry(
                        "", J, reinterpret_cast<void *>(&result))));
            }
        } break;
        case 0xad: // lreturn

        case 0x79: // lshl
        {
            auto value1 = *sf->operand_stack.top();
            sf->operand_stack.pop();
            int value2 = sf->operand_stack.top()->context_value.j & 0x1f;
            sf->operand_stack.pop();

            auto result = value1.context_value.j << value2;
            sf->operand_stack.push(std::shared_ptr<ContextEntry>(
                new ContextEntry("", J, reinterpret_cast<void *>(&result))));
        } break;
        case 0x7b: // lshr
        {
            auto value1 = *sf->operand_stack.top();
            sf->operand_stack.pop();
            int value2 = sf->operand_stack.top()->context_value.j & 0x1f;
            sf->operand_stack.pop();
            ContextEntry("", J, static_cast<void *>(&value1));
            auto result = value1.context_value.j >> value2;
            sf->operand_stack.push(std::shared_ptr<ContextEntry>(
                new ContextEntry("", J, reinterpret_cast<void *>(&result))));
        } break;
        case 0x3f: // lstore_0
        case 0x40: // lstore_1
        case 0x41: // lstore_2
        case 0x42: // lstore_3
        {
            auto index = *(byte)-0x3f;
            auto value = sf->operand_stack.top();
            sf->operand_stack.pop();
            sf->lva[index]     = value;
            sf->lva[index + 1] = value;

        } break;
        case 0x7d: // lushr
        {
            auto value1 = sf->operand_stack.top();
            sf->operand_stack.pop();
            auto value2 = sf->operand_stack.top()->context_value.i & 0x3f;
            sf->operand_stack.pop();

            auto result = value1->context_value.i >> value2;
            sf->operand_stack.push(std::shared_ptr<ContextEntry>(
                new ContextEntry("", J, reinterpret_cast<void *>(&result))));
        } break;
        case 0xc2: // monitorenter
        case 0xc3: // monitorexit

        case 0xc5: // multianewarray

        case 0xbc: // newarray

        case 0x0: // nop
            break;

        case 0x57: // pop
        {
            auto value = sf->operand_stack.top();
            sf->operand_stack.pop();
        } break;
        case 0x58: // pop2
        {
            if (category(sf->operand_stack.top()->entry_type) == 2) {
                auto value = sf->operand_stack.top();
                sf->operand_stack.pop();
            } else if (category(sf->operand_stack.top()->entry_type) == 2) {
                auto value1 = sf->operand_stack.top();
                sf->operand_stack.pop();
                auto value2 = sf->operand_stack.top()->context_value.i & 0x3f;
                sf->operand_stack.pop();
            }
        } break;
        case 0xb5: // putfield
            break;
        case 0xb3: // putstatic
            break;
        case 0xa9: // ret
            break;
        case 0xb1: // return
        {
            while (!sf->operand_stack.empty())
                sf->operand_stack.pop();
        } break;
        case 0x11: // sipush
            break;
        case 0x5f: // swap
        {
            if (category(sf->operand_stack.top()->entry_type) == 2) {
                auto value1 = sf->operand_stack.top();
                sf->operand_stack.pop();
                auto value2 = sf->operand_stack.top();
                sf->operand_stack.pop();
                sf->operand_stack.push(value2);
                sf->operand_stack.push(value1);
            }
        } break;
        case 0xaa: // tableswitch
            break;
        case 0xc4: // wide
        {
            wide = true;
        } break;
        default:
            break;
        }
    }

    return ContextEntry();
}
