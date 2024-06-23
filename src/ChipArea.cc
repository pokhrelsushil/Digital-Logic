
#include "ChipArea.h"
#include "ScreenStack.h"
#include <X11/Xlib.h>
#include <windows.h>
#include <iostream>

#define margin 30

ChipArea::ChipArea(ScreenStack *stack)
{
    this->stack = stack;
    int width = GetSystemMetrics(SM_CXSCREEN);
    int height = GetSystemMetrics(SM_CYSCREEN);

    Gtk::Box *wrapper = Gtk::manage(new Gtk::Box(Gtk::Orientation::HORIZONTAL, 0));
    wrapper->add_css_class({"chip-wrapper"});
    wrapper->set_vexpand(true);
    wrapper->set_hexpand(true);

    container = Gtk::manage(new Gtk::Grid());
    container->add_css_class({"chip-area"});
    container->set_vexpand(true);
    container->set_hexpand(true);
    // container->set_size_request(width, height);

    // creationArea
    creationArea = Gtk::manage(new Gtk::Box(Gtk::Orientation::HORIZONTAL, 0));
    creationArea->add_css_class({"creation-area"});
    container->attach(*creationArea, 0, 0, 1, 3);
    creationArea->set_vexpand(true);
    creationArea->set_hexpand(true);

    // canvas
    canvas = Gtk::manage(new Gtk::DrawingArea());
    canvas->add_css_class({"chip-canvas"});
    creationArea->append(*canvas);
    canvas->set_vexpand(true);
    canvas->set_hexpand(true);

    m_GestureDrag = Gtk::GestureDrag::create();
    m_GestureDrag->set_propagation_phase(Gtk::PropagationPhase::BUBBLE);
    m_GestureDrag->signal_drag_begin().connect(sigc::mem_fun(*this, &ChipArea::on_my_drag_begin));
    m_GestureDrag->signal_drag_end().connect(sigc::mem_fun(*this, &ChipArea::on_my_drag_end));
    m_GestureDrag->signal_drag_update().connect(sigc::mem_fun(*this, &ChipArea::on_my_drag_update));
    add_controller(m_GestureDrag);

    // connect it with canvas

    // now call the function to draw the chip
    canvas->set_draw_func(sigc::mem_fun(*this, &ChipArea::draw_on_canvas));

    draggedChip = nullptr;

    chips = new std::vector<Chip>();

    globalInputPins = new std::vector<GlobalInputPin *>();
    globalOutputPins = new std::vector<GlobalOutputPin *>();

    GlobalInputPin *globalInputPin = new GlobalInputPin(0, 30);
    GlobalInputPin *globalInputPin2 = new GlobalInputPin(1, 100);
    GlobalOutputPin *globalOutputPin = new GlobalOutputPin(0, 30);

    globalInputPins->push_back(globalInputPin);
    globalInputPins->push_back(globalInputPin2);

    std::cout << "X Global Input Pins: " << globalInputPins->size() << std::endl;

    globalOutputPins->push_back(globalOutputPin);

    create_chip(0);

    // chipSelector
    chipSelector = Gtk::manage(new ChipSelectorUI());
    chipSelector->add_css_class({"chip-selector"});
    container->attach(*chipSelector, 0, 3, 1, 1);

    // add container to the
    wrapper->append(*container);
    set_child(*wrapper);
}

void ChipArea::on_my_drag_begin(double start_x, double start_y)
{
    // std::cout << "Drag Begin" << std::endl;
    for (int i = 0; i < chips->size(); i++)
    {

        bool isInside = (*chips)[i].isMouseInside(start_x - margin, start_y - margin);
        // std::cout << "Is Inside: " << isInside << std::endl;
        if (isInside)
        {
            draggedChip = &(*chips)[i];

            // print start_x and start_y
            // std::cout << "Start X: " << start_x - margin << " Start Y: " << start_y - margin << std::endl;

            MouseOffset offset = (*chips)[i].getMouseOffset(start_x - margin, start_y - margin);
            // print the mouse offset
            // std::cout << "Offset X: " << offset.x << " Offset Y: " << offset.y << std::endl;

            double new_x = (start_x - margin) - (*chips)[i].structure->boundingBox->x - offset.x;
            double new_y = (start_y - margin) - (*chips)[i].structure->boundingBox->y - offset.y;
            // print new_x and new_y
            // std::cout << "New X: " << new_x << " New Y: " << new_y << std::endl;
            (*chips)[i].structure->boundingBox->x += new_x;
            (*chips)[i].structure->boundingBox->y += new_y;

            (*chips)[i].structure->boundingBox->intial_x += new_x;
            (*chips)[i].structure->boundingBox->intial_y += new_y;
            break;
        }
    }
    canvas->queue_draw();
}

void ChipArea::on_my_drag_update(double offset_x, double offset_y)
{

    // offset_x	X offset, relative to the start point.
    // offset_y	Y offset, relative to the start point.

    // std::cout << "Drag Update" << std::endl;
    // std::cout << "Dragged Chip [update]: " << draggedChip << std::endl;
    if (draggedChip == nullptr)
    {
        return;
    }
    // std::cout << "Offset X: " << offset_x << " Offset Y: " << offset_y << std::endl;
    // std::cout << "Mouse X: " << draggedChip->structure->boundingBox->intial_x << " Mouse Y: " << draggedChip->structure->boundingBox->intial_y << std::endl;

    double new_x = draggedChip->structure->boundingBox->intial_x + offset_x;
    double new_y = draggedChip->structure->boundingBox->intial_y + offset_y;

    draggedChip->structure->boundingBox->x = new_x;
    draggedChip->structure->boundingBox->y = new_y;

    canvas->queue_draw();
}

void ChipArea::on_my_drag_end(double offset_x, double offset_y)
{
    // std::cout << "Drag End" << std::endl;
    // use draggedChip
    if (draggedChip == nullptr)
    {
        return;
    }
    double new_x = draggedChip->structure->boundingBox->intial_x + offset_x;
    double new_y = draggedChip->structure->boundingBox->intial_y + offset_y;
    draggedChip->structure->setLoc(new_x, new_y);
    // clear draggedChip
    draggedChip = nullptr;
    // (*chips)[i].isDragging = false;
    // double new_x = (*chips)[i].structure->boundingBox->x + offset_x - (*chips)[i].structure->boundingBox->intial_x;
    // double new_y = (*chips)[i].structure->boundingBox->y + offset_y - (*chips)[i].structure->boundingBox->intial_y;
    // (*chips)[i].structure->setLoc(new_x, new_y);

    canvas->queue_draw();
}

void ChipArea::create_chip(int index)
{
    // Create AND gate
    ChipStructure *structureAND = new ChipStructure(new ChipBoundingBox{100, 100, 200, 50});
    std::vector<InputPin *> inputPins;
    std::vector<OutputPin *> outputPins;

    InputPin *inputPinAND_A = new InputPin("A", 0);
    InputPin *inputPinAND_B = new InputPin("B", 1);
    OutputPin *outputPinAND_Y = new OutputPin("Y", 0);


    globalInputPins->at(0)->bindTo(*inputPinAND_A);
    globalInputPins->at(1)->bindTo(*inputPinAND_B);

    InputPin *inputPinNOT_A = new InputPin("A", 0);


    outputPinAND_Y->bindTo(*inputPinNOT_A);



    // Create NOT gate
    ChipStructure *structureNOT = new ChipStructure(new ChipBoundingBox{100, 100, 400, 60});
    std::vector<InputPin *> inputPins2;
    std::vector<OutputPin *> outputPins2;

    OutputPin *outputPinNOT_Y = new OutputPin("Y", 0);    
    outputPinNOT_Y->bindToGlobalOutput(*globalOutputPins->at(0));

    // Connect the AND output to the NOT input to create NAND

    inputPins.push_back(inputPinAND_A);
    inputPins.push_back(inputPinAND_B);
    outputPins.push_back(outputPinAND_Y);

    inputPins2.push_back(inputPinNOT_A);
    outputPins2.push_back(outputPinNOT_Y);

    // Create chips and add to chips vector
    Chip *chipAND = new Chip(structureAND, inputPins, outputPins, "AND");
    chipAND->setChipType(ChipType::AND);
    Chip *chipNOT = new Chip(structureNOT, inputPins2, outputPins2, "NOT");
    chipNOT->setChipType(ChipType::NOT);

    chips->push_back(*chipAND);
    chips->push_back(*chipNOT);

    canvas->queue_draw();
}


void ChipArea::clear_canvas(const Cairo::RefPtr<Cairo::Context> &cr)
{
    cr->set_source_rgb(0.0, 0.0, 0.0);
    cr->fill();
}

void ChipArea::run(){
    // check all globalInputsPins
    // find all it's bind and update the PIN state

    // for(int i = 0;i < globalInputPins->size();i++){
    //     for(int j = 0;j < globalInputPins->at(i)->binds->size();j++){
    //         globalInputPins->at(i)->binds->at(j).input.state = globalInputPins->at(i)->state;
    //         if(globalInputPins->at(i)->binds->at(j).input.chip != nullptr){
    //             if(globalInputPins->at(i)->binds->at(j).input.chip->type == ChipType::NOT){
    //                 globalInputPins->at(i)->binds->at(j).input.chip->outputPins[0]->state = !globalInputPins->at(i)->state;
    //             }else if(globalInputPins->at(i)->binds->at(j).input.chip->type == ChipType::AND){
    //                 globalInputPins->at(i)->binds->at(j).input.chip->outputPins[0]->state = globalInputPins->at(i)->state && globalInputPins->at(i)->binds->at(j).input.state;
    //             }else{
    //                 globalInputPins->at(i)->binds->at(j).input.chip->outputPins[0]->state = globalInputPins->at(i)->state;
    //             }
    //         }
    //     }
    // }
}

void ChipArea::draw_on_canvas(const Cairo::RefPtr<Cairo::Context> &cr,
                              int width, int height)
{
    clear_canvas(cr);

    for (int i = 0; i < globalInputPins->size(); i++)
    {
        cr->set_source_rgb(200 / 255.0, 39 / 255.0, 92 / 255.0);
        cr->arc(globalInputPins->at(i)->radius + 10, globalInputPins->at(i)->y, globalInputPins->at(i)->radius, 0, 2 * M_PI);
        cr->fill();

        cr->set_source_rgb(20 / 255.0, 20 / 255.0, 20 / 255.0);
        cr->arc(globalInputPins->at(i)->radius + 10, globalInputPins->at(i)->y, globalInputPins->at(i)->radius / 2, 0, 2 * M_PI);
        cr->fill();

    }
    for (int i = 0; i < globalOutputPins->size(); i++)
    {
        cr->set_source_rgb(200 / 255.0, 39 / 255.0, 92 / 255.0);
        cr->arc(width - globalOutputPins->at(i)->radius - 10, globalOutputPins->at(i)->y, globalOutputPins->at(i)->radius, 0, 2 * M_PI);
        cr->fill();

        cr->set_source_rgb(20 / 255.0, 20 / 255.0, 20 / 255.0);
        cr->arc(width - globalOutputPins->at(i)->radius - 10, globalOutputPins->at(i)->y, globalOutputPins->at(i)->radius / 2, 0, 2 * M_PI);
        cr->fill();
        globalOutputPins->at(i)->x = width - globalOutputPins->at(i)->radius - 10;
    }



    for (int i = 0; i < chips->size(); i++)
    {
        (*chips)[i].draw(cr);
    }
    for (int i = 0; i < chips->size(); i++)
    {
        (*chips)[i].draw(cr);
    }

    // draw line between GlobalInputPin and input pins
    for (int i = 0; i < globalInputPins->size(); i++)
    {

        for (int j = 0; j < globalInputPins->at(i)->binds->size(); j++)
        {
            cr->set_source_rgb(20 / 255.0, 20 / 255.0, 20 / 255.0);
            cr->set_line_width(4);

            cr->move_to(globalInputPins->at(i)->radius + 10, globalInputPins->at(i)->y);
            cr->line_to(globalInputPins->at(i)->binds->at(j).input.x, globalInputPins->at(i)->binds->at(j).input.y);
            cr->stroke();
        }
    }
}

ChipSelectorUI::ChipSelectorUI()
{
    set_css_classes({"chip-selector"});
    set_orientation(Gtk::Orientation::HORIZONTAL);
    set_spacing(0);

    menu = Gtk::manage(new Gtk::Button());
    menu->set_label("Menu");
    menu->set_size_request(50, 50);
    menu->set_css_classes({"chip-menu-btn", "chip-btn"});
    append(*menu);

    for (int i = 0; i < 5; i++)
    {
        chips[i] = Gtk::manage(new Gtk::Button());
        chips[i]->set_label("Chip " + std::to_string(i));
        chips[i]->set_size_request(50, 50);
        chips[i]->set_css_classes({"chip-btn"});
        // chips[i]->signal_clicked().connect(sigc::bind<int>(sigc::mem_fun(*this, &ChipSelectorUI::on_chip_selected), i));
        append(*chips[i]);
    }
}

void ChipSelectorUI::on_chip_selected(int index)
{
    selected_chip = index;
}

Bind::Bind(InputPin &input) : input(input)
{
}

BindToGlobalOutPut::BindToGlobalOutPut(GlobalOutputPin &output) : output(output)
{
}

void Bind::printConnection()
{
    // output name and output pin x, y
    // input name and input pin x, y
    // std::cout << "Output Name: " << input.x << " Output Pin X: " << input.x << " Output Pin Y: " << input.y << std::endl;
}

Pin::Pin(std::string name, int index)
{
    this->name = name;
    this->index = index;
}

void Pin::printCord()
{
    // std::cout << "Pin X: " << x << " Pin Y: " << y << std::endl;
}

// InputPin::InputPin(std::string name, int index)
// {
//     this->name = name;
//     this->index = index;
// }

void Pin::setCord(int xP, int yP)
{
    this->x = xP;
    this->y = yP;
}

bool Pin::isInside(int mouseX, int mouseY)
{
    if (mouseX >= x - radius && mouseX <= x + radius && mouseY >= y - radius && mouseY <= y + radius)
    {
        // std::cout << "Mouse is inside the pin" << std::endl;
        return true;
    }
    return false;
}

void Pin::setRadius(int radius)
{
    this->radius = radius;
}

Cord Pin::getCord()
{
    return Cord{x, y};
}

void OutputPin::bindTo(InputPin &inputPin)
{
    Bind bind(inputPin);
    // std::cout << "address of inputPin: " <<  &inputPin << std::endl;
    binds->push_back(bind);
}

void OutputPin::bindToGlobalOutput(GlobalOutputPin &output)
{
    BindToGlobalOutPut bind(output);
    bindsToGlobalOutput->push_back(bind);
}

ChipStructure::ChipStructure(ChipBoundingBox *boundingBox)
{
    this->boundingBox = boundingBox;
    this->boundingBox->intial_x = boundingBox->x;
    this->boundingBox->intial_y = boundingBox->y;
}
void ChipStructure::setBoundingBox(int width, int height, int x, int y)
{
    this->boundingBox->width = width;
    this->boundingBox->height = height;
    this->boundingBox->x = x;
    this->boundingBox->y = y;

    this->boundingBox->intial_x = x;
    this->boundingBox->intial_y = y;
}

void ChipStructure::setLoc(int x, int y)
{
    this->boundingBox->x = x;
    this->boundingBox->y = y;
    this->boundingBox->intial_x = x;
    this->boundingBox->intial_y = y;
}

Chip::Chip(ChipStructure *structure, std::vector<InputPin *> inputPins, std::vector<OutputPin *> outputPins, std::string name)
{
    this->structure = structure;
    this->inputPins = inputPins;
    this->outputPins = outputPins;
    this->name = name;
    for(int i = 0;i < outputPins.size();i++){
        outputPins[i]->chip = this;
    }
    for(int i = 0;i < inputPins.size();i++){
        inputPins[i]->chip = this;
    }
};

void Chip::run(){
    
}

bool Chip::isMouseInside(int x, int y)
{

    // std::cout << "X: " << x << " Y: " << y << " Chip X: " << structure->boundingBox->x << " Chip Y: " << structure->boundingBox->y << " Chip Width: " << structure->boundingBox->width << " Chip Height: " << structure->boundingBox->height << std::endl;
    return x >= structure->boundingBox->x && x <= structure->boundingBox->x + structure->boundingBox->width && y >= structure->boundingBox->y && y <= structure->boundingBox->y + structure->boundingBox->height;
}

MouseOffset Chip::getMouseOffset(int x, int y)
{
    return MouseOffset{static_cast<double>(x) - structure->boundingBox->x, static_cast<double>(y) - structure->boundingBox->y};
}

void clear_canvas(const Cairo::RefPtr<Cairo::Context> &cr)
{
    cr->set_source_rgb(0.0, 0.0, 0.0);
    cr->fill();
};

void Chip::draw(const Cairo::RefPtr<Cairo::Context> &cr)
{
    // draw the chip
    // draw rectange
    // draw input and output pins
    // draw the name of the chip

    cr->set_source_rgb(38 / 255.0, 38 / 255.0, 38 / 255.0);
    // calculate height using the max(height of input pins, height of output pins)
    // int gapper = 40;
    // int inputHeight = inputPins->size() * gapper;
    // int outputHeight = outputPins->size() * gapper;
    // int height = inputHeight > outputHeight ? inputHeight : outputHeight;
    // cr->set_source_rgb(6/255, 39/255, 92/255);
    // cr->rectangle(0, 0, 100, height);
    // cr->fill();

    int x = structure->boundingBox->x;
    int y = structure->boundingBox->y;

    int gapper = 25;
    int width = 150;
    int inputHeight = inputPins.size() * gapper;
    int outputHeight = outputPins.size() * gapper;
    int height = inputHeight > outputHeight ? inputHeight : outputHeight;

    structure->boundingBox->width = width;
    structure->boundingBox->height = height;

    cr->set_source_rgb(200 / 255.0, 39 / 255.0, 92 / 255.0);
    cr->rectangle(x, y, width, height);
    cr->fill();

    // draw the input pins
    int eachPinSpace = (height / (inputPins.size() > outputPins.size() ? inputPins.size() : outputPins.size())) / 2;

    for (int i = 0; i < inputPins.size(); i++)
    {

        cr->set_source_rgb(20 / 255.0, 20 / 255.0, 20 / 255.0);
        // cr->arc(20, i * gapper + gapper / 2, 5, 0, 2 * M_PI);
        int inputPinEachHeight = height / inputPins.size();
        cr->arc(x, y + i * (inputPinEachHeight) + (inputPinEachHeight / 2), eachPinSpace - 3, 0, 2 * M_PI);

        cr->fill();

        cr->set_source_rgb(1.0, 1.0, 1.0);
        cr->move_to(x + 15, y + i * (inputPinEachHeight) + (inputPinEachHeight / 2) + 3);
        cr->set_font_size(14);
        cr->show_text(inputPins[i]->name);
        inputPins[i]->setCord(x, y + i * (inputPinEachHeight) + (inputPinEachHeight / 2));
        // std::cout << "Input Pin:" << std::endl;
        inputPins[i]->printCord();
        // std::cout << "x:" << inputPins[i]->x << " y:" << inputPins[i]->y << std::endl;
    }

    // draw the output pins
    for (int i = 0; i < outputPins.size(); i++)
    {
        cr->set_source_rgb(20 / 255.0, 20 / 255.0, 20 / 255.0);
        // cr->arc(80, i * gapper + gapper / 2, 5, 0, 2 * M_PI);
        int outputPinEachHeight = height / outputPins.size();
        cr->arc(x + width, y + i * (outputPinEachHeight) + (outputPinEachHeight / 2), eachPinSpace - 3, 0, 2 * M_PI);
        cr->fill();

        cr->set_source_rgb(1.0, 1.0, 1.0);
        cr->move_to(x + width - 25, y + i * (outputPinEachHeight) + (outputPinEachHeight / 2) + 3);
        cr->set_font_size(14);
        cr->show_text(outputPins[i]->name);
        outputPins[i]->setCord(x + width, y + i * (outputPinEachHeight) + (outputPinEachHeight / 2));
        // std::cout << "Output Pin:" << std::endl;
        outputPins[i]->printCord();

        // std::cout << "x:" << outputPins[i]->x << " y:" << outputPins[i]->y << std::endl;
        // std::cout << "Size:" << outputPins[i]->binds->size() << std::endl;
        if (outputPins[i]->binds->size() > 0)
        {
            // std::cout << "Bind \n";

            for (int j = 0; j < outputPins[i]->binds->size(); j++)
            {
                // std::cout << "address of inputPin: " << outputPins[i]->binds->at(j).input << std::endl;
                outputPins[i]->binds->at(j).printConnection();
            }
        }
    }

    // draw line between input and output pins
    for (int i = 0; i < outputPins.size(); i++)
    {
        for (int j = 0; j < outputPins[i]->binds->size(); j++)
        {
            cr->set_source_rgb(20 / 255.0, 20 / 255.0, 20 / 255.0);
            cr->set_line_width(4);

            cr->move_to(outputPins[i]->x, outputPins[i]->y);
            cr->line_to(outputPins[i]->binds->at(j).input.x, outputPins[i]->binds->at(j).input.y);
            cr->stroke();
        }
    }

    // draw line between output and Global output pins
    for (int i = 0; i < outputPins.size(); i++)
    {
        for (int j = 0; j < outputPins[i]->bindsToGlobalOutput->size(); j++)
        {
            cr->set_source_rgb(20 / 255.0, 20 / 255.0, 20 / 255.0);
            cr->set_line_width(4);
            // width - globalOutputPins->at(i)->radius - 10
            cr->move_to(outputPins[i]->x, outputPins[i]->y);
            cr->line_to(outputPins[i]->bindsToGlobalOutput->at(j).output.x, outputPins[i]->bindsToGlobalOutput->at(j).output.y);
            cr->stroke();
        }
    }


    // draw the name of the chip
    cr->set_source_rgb(1.0, 1.0, 1.0);
    int font_size = 20;
    int length = name.length();
    int nameLengthInPixels = length * font_size;
    cr->set_font_size(font_size);

    cr->move_to(x + (width / 2) - 20, y + (height / 2) + 7);
    cr->show_text(name);
    cr->fill();
}


void GlobalInputPin::bindTo(InputPin &inputPin)
{
    Bind bind(inputPin);
    binds->push_back(bind);
}