/*
 * 
 */

#include "interface.h"
#include "routeInstruction.h"
#include "graphics.h"
#include "load_map.h"
using namespace std;

Loaded_Surface surface_collection;

Buttons all_buttons;

infoType draw;

bool existPin;

double raw_zoom, zoom_level;

extern bool isTree;
//extern bool loadAPI;

extern vector<unsigned> thePath;

extern vector<unsigned> highlight;
static int intersectionToHighlight = -1;
//extern  std::vector <unsigned > path2;
//calls according functions when mouse is clicked 
PathInterface clickPath_en;

vector <unsigned> click_path;
bool draw_help = false;

void act_on_mousebutton(float x, float y, t_event_buttonPressed button_info) {
    set_drawing_buffer(OFF_SCREEN);
    //left click
    if (button_info.button == 1) {
        // Check if the user is checking on candidates boxes
        if (search_bar_manager.mouseClickReact(x, y, button_info))
            ;
            // If mouse click is not on search bar

        else {
            if (!react_to_find_closest(x, y))
                react_to_highlight(x, y);
            react_to_restriction(x, y);
        }
        drawscreen();

        //right click
    } else if (button_info.button == 3) {

        if (search_bar.mode == highlightStreet)
            search_bar.clearHighlights();

        else if (search_bar.mode == highlightIntersection)
            search_bar.clearPins();

        intersectionToHighlight = -1;

        information_bar.first = Do_not_show;

        landing_poi.draw = false;

        drawscreen();
    }


}

//

void act_on_mousemove(float x, float y) {
    if (search_bar.activated || search_bar_route.activated ||
            (search_bar_manager.isFindingRoute() &&
            search_bar_manager.isBothBarLocked() &&
            !search_bar_manager.clickPath.enable)) {
        x = xworld_to_scrn(x);
        y = yworld_to_scrn(y);
        search_bar_manager.mouseMoveReact(x, y);

        drawscreen();
    }
}

#include <X11/keysym.h>
#include <bits/shared_ptr_base.h>
#include <iterator>

void act_on_keypress(char key_pressed, int keysym) {
    // Determine the active searchBar
    searchBar* acceptor = search_bar_manager.inputAcceptor();

    if (acceptor == NULL)
        return;

    if (search_bar_manager.clickPath.enable)
        return;

    switch (keysym) {
        case XK_BackSpace:

            acceptor->backspace();
            break;

        case XK_Return:

            if (!search_bar_manager.isFindingRoute()) {
                if (acceptor->mode == streetInSearch) {
                    acceptor->searchStreet();
                    acceptor->update();

                } else if (acceptor->mode == intersectionInSearch) {
                    acceptor->searchIntersection();
                    acceptor->update();
                } else if (acceptor->mode == POIInSearch) {
                    acceptor->searchPOIs();
                    acceptor->update();
                }
            } else {
                search_bar_manager.lockSearchBar(acceptor);
                if (search_bar_manager.isBothBarLocked())
                    search_bar_manager.showUpFindButton();
            }
            break;

        case XK_Up:
            acceptor->shiftUpSelection();
            acceptor->updateCandidatesStyle();

            break;

        case XK_Down:
            acceptor->shiftDownSelection();
            acceptor->updateCandidatesStyle();

            break;

        case XK_Tab:
            acceptor->autoComplete();
            break;

        default:
            if (acceptor->activated)
                acceptor->input(key_pressed);
            break;
    }

    drawscreen();
}

void drawscreen() {

    clearscreen();
    
    set_drawing_buffer(OFF_SCREEN);

    setcolor(238, 236, 230, 255);
    fillrect(world.left, world.bottom, world.right, world.top);
    //The if statements below are for street widths adjustment with zooming levels
    raw_zoom = (get_visible_world().top() - get_visible_world().bottom()) / (fabs(world.top - world.bottom));



    zoom_level = raw_zoom * (world.top - world.bottom) / MAP_SIZE_REFERENCE;

    int zooming_level = 0;
    // Determine zoom level
    if (zoom_level > 0.7)
        zooming_level = 0;
    else if (zoom_level < 0.7 && zoom_level > 0.4)
        zooming_level = 1;
    else if (zoom_level < 0.4 && zoom_level > 0.25)
        zooming_level = 2;
    else if (zoom_level < 0.25 && zoom_level > 0.15)
        zooming_level = 3;
    else if (zoom_level < 0.15 && zoom_level > 0.1)
        zooming_level = 4;
    else if (zoom_level < 0.1 && zoom_level > 0.01)
        zooming_level = 5;
    else if (zoom_level < 0.01)
        zooming_level = 6;

    setlinestyle(SOLID, ROUND);

    draw_features(-1, -1);

    draw_street(zooming_level);

    draw_poi();

    // In animation, do not draw those to make animation smoother
    if (!animation_manager.isInAnimation()) {
        draw_poi();

        draw_segment_names(zooming_level);

    }
        
    // Highlight streets, if searched
    {
        search_bar_manager.drawHighlights(zooming_level);
    }

    // Drawing search bar
    {
        search_bar_manager.drawSearchBar();
    }

    if (intersectionToHighlight != -1)
        highlight_intersection();

    if (information_bar.first != Do_not_show)
        draw_information_bar();

    if (landing_poi.draw) {

        // Get the previous color
        auto previous_color = getcolor();

        setcolor(BLACK);

        float x_screen = xworld_to_scrn(landing_poi.x);
        float y_screen = yworld_to_scrn(landing_poi.y);
        set_coordinate_system(GL_SCREEN);

        draw_surface(surface_collection.poi, x_screen - 10, y_screen - 20);
        t_bound_box box(x_screen - 100, y_screen, x_screen + 100, y_screen + 20);
        setfontsize(28);
        drawtext_in(box, landing_poi.info);
        setfontsize(10);
        set_coordinate_system(GL_WORLD);

        setcolor(previous_color);
    }

    if (!click_path.empty())
        highlightStreets(click_path, zooming_level);
    if (draw_help) {

        set_coordinate_system(GL_SCREEN);
        draw_surface(surface_collection.user_manual, get_visible_screen().get_center().x-650, get_visible_screen().get_center().y-365);
        set_coordinate_system(GL_WORLD);
    }

    copy_off_screen_buffer_to_screen();
}

bool react_to_find_closest(double x, double y) {
    //find closest thing user clicks on
    coord click = make_pair(x, y);
    vector<value> close_intersect = closestIntersec(click);
    vector<value> close_poi = closestPOI(click);

    coord intersectPt = latlon_into_cartesian(getIntersectionPosition(close_intersect[0].second));
    coord POIpt = latlon_into_cartesian(getPointOfInterestPosition(close_poi[0].second));

    double poi_dist = find_cartesian_distance_squared(click, POIpt);
    double intersect_dist = find_cartesian_distance_squared(click, intersectPt);

    intersectionToHighlight = -1;

    string info;
    if (poi_dist > 0.00000006 && intersect_dist > 0.00000003) {
        draw = Do_not_show;
        return false;
    } else {
        unsigned result;
        if (poi_dist > intersect_dist) {
            information_bar.first = Intersection;
            result = close_intersect[0].second;
            info = getIntersectionName(result);
            draw = Intersection;
            react_to_landing_poi(x, y);
            if (zoom_level < 0.05)
                intersectionToHighlight = result;
        } else {

            draw = POI;

            result = close_poi[0].second;
            info = getPointOfInterestName(result);
            string type = getPointOfInterestType(result);
            bool appear = (*getPtrToPOISet())[result].draw;
            if ((type != "university" && type != "hospital" && type != "restaurant" &&
                    type != "fast_food" && type != "bank" && type != "fuel" &&
                    type != "parking") || !appear) {
                draw = Do_not_show;

            }
            information_bar.first = draw;
        }
        //give info about whether to draw and what to write
        information_bar = make_pair(draw, info);
    }

    if (information_bar.first != Do_not_show)
        drawscreen();

    return true;
}

void react_to_highlight(float x, float y) {

    draw_features(x, y);

}

void draw_segment_names(unsigned zooming_level) {

    auto curWorld = get_visible_world();

    for (unsigned streetSegmentIndex = 0; streetSegmentIndex < getNumberOfStreetSegments(); streetSegmentIndex++) {

        // fasten up statements
        if (!curWorld.intersects(segVec[streetSegmentIndex].ptsOnStreet.front().first,
                segVec[streetSegmentIndex].ptsOnStreet.front().second)
                )
            continue;
        if (!curWorld.intersects(segVec[streetSegmentIndex].ptsOnStreet.back().first,
                segVec[streetSegmentIndex].ptsOnStreet.back().second)
                )
            continue;

        string Seg_level = segVec[(segLvlookUp[streetSegmentIndex]).second].level;

        auto scale_factor = getScaleFactor(zooming_level, Seg_level);

        if (!scale_factor)
            continue;

        if (zoom_level > 0.25)
            return;
        else if (zoom_level < 0.25 && zoom_level > 0.15)
            setfontsize(6);
        else if (zoom_level < 0.15 && zoom_level > 0.1)
            setfontsize(7);
        else if (zoom_level < 0.1 && zoom_level > 0.01)
            setfontsize(8);
        else if (zoom_level < 0.01)
            setfontsize(9);
        else if (zoom_level < 0.005)
            setfontsize(10);
        else if (zoom_level < 0.002)
            setfontsize(12);

        if ((*getPtrToStSegSet()) [streetSegmentIndex].drawn) {

            string seg_level = (*getPtrToStSegSet()) [streetSegmentIndex].level;
            string seg_name = (*getPtrToStSegSet()) [streetSegmentIndex].streetName;
            bool isOneway = (*getPtrToStSegSet()) [streetSegmentIndex].info.oneWay;
            //this street is messed up
            if (seg_name == "Temperance Street")
                continue;
            for (auto iter_street = (*getPtrToStSegSet()) [streetSegmentIndex].ptsOnStreet.begin();
                    (iter_street + 1) != (*getPtrToStSegSet()) [streetSegmentIndex].ptsOnStreet.end();
                    iter_street++) {
                if (seg_name == "<unknown>")
                    continue;
                float angle = 0;
                t_bound_box nameBox;
                //     t_bound_box onewayBox;
                double startx, starty, endx, endy;
                startx = (*iter_street).first;
                starty = (*iter_street).second;
                endx = (*(iter_street + 1)).first;
                endy = (*(iter_street + 1)).second;
                //from bot left to top right
                if (startx < endx && starty < endy) {
                    t_bound_box temp(startx, starty, endx, endy);
                    nameBox = temp;
                    if (isOneway && iter_street == (*getPtrToStSegSet()) [streetSegmentIndex].ptsOnStreet.begin()) {
                        seg_name.append("      -->  ");
                    }
                    angle = atan((endy - starty) / (endx - startx));
                }//from bot right to top left
                else if (startx > endx && starty < endy) {
                    t_bound_box temp(endx, starty, startx, endy);
                    nameBox = temp;
                    if (isOneway && iter_street == (*getPtrToStSegSet()) [streetSegmentIndex].ptsOnStreet.begin()) {
                        seg_name.append("      <--  ");
                    }
                    angle = atan((endy - starty) / (endx - startx));

                }//from top right to bottom left
                else if (startx > endx && starty > endy) {
                    t_bound_box temp(endx, endy, startx, starty);
                    nameBox = temp;
                    if (isOneway && iter_street == (*getPtrToStSegSet()) [streetSegmentIndex].ptsOnStreet.begin()) {
                        seg_name.append("      <--  ");
                    }
                    angle = atan((endy - starty) / (endx - startx));
                } else if (startx < endx && starty > endy) {
                    t_bound_box temp(startx, endy, endx, starty);
                    nameBox = temp;
                    if (isOneway && iter_street == (*getPtrToStSegSet()) [streetSegmentIndex].ptsOnStreet.begin()) {
                        seg_name.append("      -->  ");
                    }
                    angle = atan((endy - starty) / (endx - startx));
                }

                settextrotation(angle / DEG_TO_RAD);

                setcolor(BLACK);

                drawtext_in(nameBox, seg_name);

                settextrotation(0);
            }
            //      (*getPtrToStSegSet())[streetSegmentIndex].drawn=false;
        }
    }

}

void draw_information_bar() {

    set_coordinate_system(GL_SCREEN);
    setfontsize(10);
    setlinewidth(2);
    // Get the previous color
    auto previous_color = getcolor();

    setcolor(0, 0, 0, 255 * 0.25);
    drawrect(598, 873, 1002, 927);
    setcolor(BLACK_50A);
    drawrect(599, 874, 1001, 926);
    setcolor(predefined_styles[WHITE_100A]);
    fillrect(600, 875, 1000, 925);
    setcolor(BLACK);
    drawtext(800, 900, information_bar.second);
    set_coordinate_system(GL_WORLD);

    // set color back
    setcolor(previous_color);
}

void draw_street(double zooming_level) {
    //drawing all streets
    vector < t_bound_box > boxes;
    vector < string > refs;

    float scale_factor;

    auto curWorld = get_visible_world();

    float tolerance = 0.1 / (zooming_level + 5);

    auto toleranceWorld = t_bound_box(curWorld.left() - tolerance*screen_ratio_width,
            curWorld.bottom() - tolerance*screen_ratio_height,
            curWorld.right() + tolerance*screen_ratio_width,
            curWorld.top() + tolerance * screen_ratio_height);
    
    for (unsigned streetSegmentIndex = 0; streetSegmentIndex < segLvlookUp.size(); streetSegmentIndex++) {

        if (!toleranceWorld.intersects(segVec[(segLvlookUp[streetSegmentIndex]).second].ptsOnStreet.front().first,
                segVec[(segLvlookUp[streetSegmentIndex]).second].ptsOnStreet.front().second))
            continue;

        if (!toleranceWorld.intersects(segVec[(segLvlookUp[streetSegmentIndex]).second].ptsOnStreet.back().first,
                segVec[(segLvlookUp[streetSegmentIndex]).second].ptsOnStreet.back().second))
            continue;

        string seg_level = segVec[(segLvlookUp[streetSegmentIndex]).second].level;

        scale_factor = getScaleFactor(zooming_level, seg_level);
        //test if its worth drawing
        //if (segVec[(segLvlookUp[streetSegmentIndex]).second].from==2)
        //  continue;

        // Draw street segments only when the determined factor is not 0
        if (scale_factor != 0) {
            setlinewidth(scale_factor / zoom_level);

            if (seg_level == "motorway" || seg_level == "trunk")
                setcolor(224, 190, 95, 255);
            else
                setcolor(WHITE);
        } else
            continue;

        //drawing segments by curve points
        for (auto iter_street = segVec[(segLvlookUp[streetSegmentIndex]).second].ptsOnStreet.begin();
                (iter_street + 1) != segVec[(segLvlookUp[streetSegmentIndex]).second].ptsOnStreet.end();
                iter_street++) {

            StreetSegClt currentSegment = segVec[(segLvlookUp[streetSegmentIndex]).second];
            double startx, starty, endx, endy;
            startx = (*iter_street).first;
            starty = (*iter_street).second;
            endx = (*(iter_street + 1)).first;
            endy = (*(iter_street + 1)).second;

            drawline(startx, starty, endx, endy);

            if (streetSegmentIndex % 300 == 0 && zooming_level < 5 && currentSegment.levelNum > 5) {

                unsigned numPts = currentSegment.ptsOnStreet.size();
                double midPointx = 0.5 * (currentSegment.ptsOnStreet[0].first +
                        currentSegment.ptsOnStreet[numPts - 1].first);
                double midPointy = 0.5 * (currentSegment.ptsOnStreet[0].second +
                        currentSegment.ptsOnStreet[numPts - 1].second);


                t_bound_box temp(midPointx - 0.008 * zoom_level, midPointy - 0.004 * zoom_level,
                        midPointx + 0.008 * zoom_level, midPointy + 0.004 * zoom_level);
                refs.push_back(currentSegment.ref);
                boxes.push_back(temp);

            }

        }
        segVec[(segLvlookUp[streetSegmentIndex]).second].drawn = true;

    }

    auto prevStyle = getcolor();
    if (zooming_level < 5) {
        for (unsigned i = 0; i < boxes.size(); i++) {

            if (boxes[i].intersects(boxes[i].get_center())) {

                setcolor(WHITE);
                setfontsize(10);
                setlinewidth(1);
                fillrect(boxes[i]);
                setcolor(predefined_styles[BLACK_50A]);
                drawrect(boxes[i]);
                setcolor(BLACK);
                drawtext_in(boxes[i], refs[i], 0.1);
                setfontsize(10);
            }
        }
    }


    setcolor(prevStyle);
}

void draw_features(float x, float y) {

    if (fabs(x + 1) < 0.0001 && fabs(y + 1) < 0.0001) {

        for (auto iter_feature = getPtrToFeatureSet()->begin();
                iter_feature != getPtrToFeatureSet()->end();
                iter_feature++) {


            switch (iter_feature->type) {

                case Park:
                    setcolor(188, 228, 153, 255);
                    break;
                case Beach:
                    setcolor(251, 248, 174, 255);
                    break;
                case Lake:
                    setcolor(144, 200, 249, 255);
                    break;
                case River:
                    setcolor(135, 206, 235, 255);
                    setlinewidth(1 + 0.11 / zoom_level);
                    break;
                case Island:
                    setcolor(238, 236, 230, 255);
                    break;

                case Shoreline:
                    setcolor(166, 215, 133, 255);
                    break;

                case Building:
                    setcolor(243, 229, 171, 255);
                    break;

                case Greenspace:
                    setcolor(188, 228, 153, 255);
                    break;

                case Golfcourse:
                    setcolor(188, 228, 153, 255);
                    break;

                case Stream:
                    setcolor(135, 206, 235, 255);
                    break;
                default:
                    setcolor(WHITE);


            }

            unsigned numFeaturePts = iter_feature->pts_on_feature.size();
            //   shoreline is a corner case
            if (iter_feature->type == Lake) {

                //to be used for draw poly
                //t_point* t_pointer;
                unsigned length = iter_feature->pointCount + 1;
                //finite length array
                t_point* array = new t_point[length + 1];

                unsigned i = 0;
                for (auto t_p = iter_feature->pts_on_feature.begin(); t_p != iter_feature ->pts_on_feature.end(); t_p++) {

                    t_point k(t_p->first, t_p->second);
                    array[i] = k;
                    i++;
                }

                t_point k(iter_feature->pts_on_feature.begin()->first, iter_feature->pts_on_feature.begin()->second);
                array[i] = k;
                //t_pointer = array;

                fillpoly(array, iter_feature->pointCount);
                delete[] array;

            }

            //closed shape
            if ((iter_feature->pts_on_feature[0].first == iter_feature->pts_on_feature[numFeaturePts - 1].first &&
                    iter_feature->pts_on_feature[0].second == iter_feature->pts_on_feature[numFeaturePts - 1].second)) {

                if (iter_feature->type == Building && zoom_level > 0.1)
                    continue;

                //to be used for draw poly

                unsigned length = iter_feature->pointCount;

                //finite length array
                t_point* array = new t_point[length];

                unsigned i = 0;
                for (auto t_p = iter_feature->pts_on_feature.begin(); t_p != iter_feature ->pts_on_feature.end(); t_p++) {

                    t_point k(t_p->first, t_p->second);
                    array[i] = k;
                    i++;
                }



                fillpoly(array, iter_feature->pointCount);
                delete []array;

            } else {

                for (unsigned p = 0; p < numFeaturePts - 1; p++) {

                    drawline(iter_feature->pts_on_feature[p].first, iter_feature->pts_on_feature[p].second,
                            iter_feature->pts_on_feature[p + 1].first, iter_feature->pts_on_feature[p + 1].second);

                }
            }
        }
    } else {
        //drawing highlight of a feature iff its x,y is inside the closed shape
        react_to_landing_poi(x, y);
        for (auto iter_feature = getPtrToFeatureSet()->begin();
                iter_feature != getPtrToFeatureSet()->end();
                iter_feature++) {

            //determine color
            switch (iter_feature->type) {

                case Park:
                    setcolor(166, 215, 133, 255);
                    break;
                case Beach:
                    setcolor(255, 238, 173, 255);
                    break;
                case Lake:
                    setcolor(135, 206, 235, 255);
                    break;
                case River:
                    setcolor(135, 206, 235, 255);
                    break;
                case Island:
                    setcolor(252, 194, 0, 255);
                    break;

                case Shoreline:
                    setcolor(166, 215, 133, 255);
                    break;

                case Building:
                    setcolor(245, 222, 179, 255);
                    break;

                case Greenspace:
                    setcolor(245, 222, 179, 255);
                    break;

                case Golfcourse:
                    setcolor(245, 222, 179, 255);
                    break;

                case Stream:
                    setcolor(135, 206, 235, 255);
                    break;
                default:
                    setcolor(RED);

            }

            unsigned numFeaturePts = iter_feature->pts_on_feature.size();

            if ((iter_feature->pts_on_feature[0].first == iter_feature->pts_on_feature[numFeaturePts - 1].first &&
                    iter_feature->pts_on_feature[0].second == iter_feature->pts_on_feature[numFeaturePts - 1].second)) {

                if (iter_feature->type == Building && zoom_level > 0.1)
                    continue;

                //to be used for draw poly

                unsigned length = iter_feature->pointCount;

                //finite length array
                t_point* t_pointer = new t_point [length];

                unsigned i = 0;
                for (auto t_p = iter_feature->pts_on_feature.begin(); t_p != iter_feature ->pts_on_feature.end(); t_p++) {

                    t_point k(t_p->first, t_p->second);
                    t_pointer[i] = k;
                    i++;
                }

                //determining if clicked in the shape
                float distanceAlongVector, yDistance;
                int endIndex;
                int sidesToLeft = 0;
                t_point vecOfSide;
                for (unsigned istart = 0; istart < length; istart++) {
                    endIndex = (istart + 1) % length; // Next vertex after istart
                    vecOfSide = t_point(t_pointer[endIndex].x - t_pointer[istart].x,
                            t_pointer[endIndex].y - t_pointer[istart].y);

                    yDistance = (y - t_pointer[istart].y);
                    if (vecOfSide.y != 0) {
                        distanceAlongVector = yDistance / vecOfSide.y;
                    } else if (yDistance == 0) {
                        distanceAlongVector = 0;
                    } else {
                        distanceAlongVector = 1e10; // Really infinity, but this is big enough
                    }

                    // We intersect this side if the distance (scaling) along the side vector to
                    // get to our "click" y point is between 0 and 1.  Count the first
                    // endpoint of the side as being part of the line (distanceAlongVector = 0)
                    // but don't count the second endPoint; we'll intersect that point later
                    // when we check the next side.
                    if (distanceAlongVector >= 0 && distanceAlongVector < 1) {
                        float xIntersection = t_pointer[istart].x + vecOfSide.x * distanceAlongVector;
                        if (xIntersection <= x)
                            sidesToLeft++;
                    }
                }
                if (sidesToLeft == 1) {
                    setcolor(255, 255, 0, 255 * 0.25);
                    fillpoly(t_pointer, iter_feature->pointCount);

                    if (iter_feature->name == "<noname>") {
                        information_bar = make_pair(Feature, "Sorry, information not yet available");
                    } else {
                        information_bar = make_pair(Feature, iter_feature->name);
                    }

                    if (information_bar.first != Do_not_show) {
                        draw_information_bar();
                    }
                    break;
                } else
                    information_bar.first = Do_not_show;
            }
        }

    }
}

void drawCandidates(vector<candidateRect> candiVec) {
    for (auto iter = candiVec.begin(); iter != candiVec.end(); iter++) {
        setlinestyle(SOLID, BUTT);
        setcolor(iter->rect_style);

        drawrect(iter->rect);
        fillrect(iter->rect);

        setcolor(iter->text_style);
        drawtext_in(iter->rect, iter->text);
    }
}

void draw_poi() {
    for (auto iter_poi = getPtrToPOISet()->begin(); iter_poi != getPtrToPOISet()->end();
            iter_poi++) {
        if (iter_poi->draw) {
            if (iter_poi->type == "university" && zoom_level < 0.1) {

                draw_surface(surface_collection.uni, iter_poi->cartesian_p.first, iter_poi->cartesian_p.second);
            } else if (iter_poi->type == "hospital" && zoom_level < 0.05) {

                draw_surface(surface_collection.hospital, iter_poi->cartesian_p.first, iter_poi->cartesian_p.second);
            } else if (iter_poi->type == "restaurant" && zoom_level < 0.01) {

                draw_surface(surface_collection.restaurant, iter_poi->cartesian_p.first, iter_poi->cartesian_p.second);
            } else if (iter_poi->type == "fast_food" && zoom_level < 0.01) {

                draw_surface(surface_collection.fast_food, iter_poi->cartesian_p.first, iter_poi->cartesian_p.second);
            } else if (iter_poi->type == "bank" && zoom_level < 0.02) {

                draw_surface(surface_collection.bank, iter_poi->cartesian_p.first, iter_poi->cartesian_p.second);
            } else if (iter_poi->type == "fuel" && zoom_level < 0.02) {

                draw_surface(surface_collection.fuel, iter_poi->cartesian_p.first, iter_poi->cartesian_p.second);
            } else if (iter_poi->type == "parking" && zoom_level < 0.01) {

                draw_surface(surface_collection.parking, iter_poi->cartesian_p.first, iter_poi->cartesian_p.second);
            }
            if (zoom_level < 0.01 && iter_poi->type != "university" && iter_poi->type != "hospital"
                    && iter_poi->type != "restaurant" && iter_poi->type != "fast_food"
                    && iter_poi->type != "bank" && iter_poi->type != "fuel" &&
                    iter_poi->type != "parking")
                draw_surface(surface_collection.allpoi, iter_poi->cartesian_p.first, iter_poi->cartesian_p.second);
        }
    }
}

/******************* Implementations of search bar manager *********************/


searchBarManager::searchBarManager(searchBar* _main, searchBar* _sub) :
clickPath(PathInterface()),findRoute(false), main(_main), sub(_sub),button_x(330.),
 button_y(10.), findButtonOut(false), instructions(routeInstruction()) {
    backgroundStyle = t_color(255, 255, 255, 122);
    background = t_bound_box(t_point(button_x, button_y), 40., 40.);
}

void searchBarManager::drawSearchBar() {
    int previous_font_size = getfontsize();

    setfontsize(10);

    // If in mode of finding route, draw both search bar
    if (findRoute) {
        main->drawself();
        sub->drawself();
    }// else do not do the sub search bar
    else {
        main->drawself();
    }

    set_coordinate_system(GL_SCREEN);

    auto colorBefor = getcolor();

    float widthExtended = main->search_bar_width - main->orginalWidth;

    // Update sub search coords
    sub->search_bar_x = button_x + 110. + widthExtended;

    // Update background coords
    background = t_bound_box(t_point(button_x + widthExtended, button_y), 40., 40.);
    // Update clear button coords
    crossBackground = t_bound_box(t_point(button_x + widthExtended + 50., button_y), 40., 40.);

    setcolor(backgroundStyle);
    drawrect(background);
    fillrect(background);

    setcolor(predefined_styles[WHITE_100A]);
    drawrect(crossBackground);
    fillrect(crossBackground);

    draw_surface(surface_collection.route, button_x + 4 + widthExtended, button_y + 2);
    draw_surface(surface_collection.cross, button_x + 5 + widthExtended + 50., button_y + 5);

    // If both search bars are locked, draw "find route" button
    if (isBothBarLocked()) {
        setcolor(findRouteButtonStyle);
        drawrect(findRouteButton);
        fillrect(findRouteButton);
        setcolor(0, 0, 0, findRouteButtonTransparency);
        drawtext_in(findRouteButton, "FIND");
    }

    if (inFindRoute) {

        drawInstructions();
    }

    setcolor(colorBefor);

    set_coordinate_system(GL_WORLD);
    setfontsize(previous_font_size);
}

void searchBarManager::drawHighlights(float zooming_level) {
    if (!findRoute) {
        if (main->mode == highlightStreet) {
            highlightStreets(streetResult, zooming_level, HIGHLIGHT);
        } else if (main->mode == highlightIntersection) {

            for (auto cltIter = intersectionResult.begin(); cltIter != intersectionResult.end(); cltIter++) {
                float curX = xworld_to_scrn(cltIter->x);
                float curY = yworld_to_scrn(cltIter->y);

                set_coordinate_system(GL_SCREEN);

                draw_surface(surface_collection.intersectionPin, curX - 19, curY - 60);
            }

            set_coordinate_system(GL_WORLD);
        } else if (main->mode == highlightPOI) {

            for (auto &eachPOI : POIResult)

                draw_surface(surface_collection.poi, eachPOI.x, eachPOI.y);
        }
    } else {
        if (main->bar_color == predefined_styles[LOCKED]) {
            pinPlacer(from);
        }

        if (sub->bar_color == predefined_styles[LOCKED])
            if (searchFor == interToInter) {
                pinPlacer(to);
            }

        if (inFindRoute) {
            highlightStreets(routeFound, zooming_level);
        }
    }
}

void searchBarManager::mouseMoveReact(float x, float y) {
    // Check whether the search engine is in route finding mode
    if (!findRoute) {
        if (main->activated) {
            main->updateCandidatesStyle(x, y);
        }
    } else {
        if (main->activated) {
            main->updateCandidatesStyle(x, y);
        } else if (sub->activated) {
            sub->updateCandidatesStyle(x, y);
        }
    }
    //    buttonMouseMoveReact(x, y);
    findButtonMouseMoveReact(x, y);
}

bool searchBarManager::mouseClickReact(float x, float y, t_event_buttonPressed button_info) {
    searchBar* active = NULL;
    float x_scrn = xworld_to_scrn(x), y_scrn = yworld_to_scrn(y);

    // if clicked on the cross button 
    if (crossBackground.intersects(x_scrn, y_scrn)) {
        clearUpSearchBars();
        return true;
    }
    int useful = button_info.button;
    useful ++;
    // click path mode will have higher priority than others
    if (clickPath.enable) {
        auto currentClick = clickPath.numClick;
        switch (currentClick) {
            case firstClick:
            {
                vector<value> request = closestIntersec(coord(x, y));
                clickPath.intx_1 = request[0].second;
                from = clickPath.intx_1;
                clickPath.numClick = secondClick;

                updateSearchBarsInput(main, getIntersectionName(clickPath.intx_1), BLACK_100A, LOCKED);
            }
                break;

            case secondClick:
            {
                vector<value> request = closestIntersec(coord(x, y));
                clickPath.intx_2 = request[0].second;
                to = clickPath.intx_2;

                searchFor = interToInter;

                updateSearchBarsInput(sub, getIntersectionName(clickPath.intx_2), BLACK_100A, LOCKED);

                // copy result to self
                searchRoute();

                // disable clicking response, until user click on cross
                clickPath.numClick = -1;

                update_message("Query Path Off");

            }
                break;

            default:
                ;
        }
        return true;
    }

    if (inFindRoute) {

    }

    // if in find route clicked on either search bar
    if (findRoute) {
        if (main->checkActivation(x_scrn, y_scrn))
            active = main;

        if (sub->checkActivation(x_scrn, y_scrn))
            active = sub;


        if (active != NULL) {
            if (active->mode != managerControlled) {
                lockSearchBar(active);
                drawscreen();
                return true;
            } else {
                resetSearchBar(active);
                active->activated = true;
                active->bar_color = predefined_styles[WHITE_100A];
                // update displaying after been reset
                drawSearchBar();
                main->update();
                sub->update();
                return true;
            }
        }

        if (search_bar_manager.isBothBarLocked())
            search_bar_manager.showUpFindButton();

        // if clicked on the find route button
        if (background.intersects(x_scrn, y_scrn)) {
            backgroundStyle = predefined_styles[WHITE_50A];
            animation_manager.invoke_animation(WARP_UP_SEARCH_BAR);
            // Don't move the statement below, otherwise animation wouldn't
            // be drew
            this->findRoute = false;
            return true;
        }

        // if if clicked on the find button
        if (findRouteButton.intersects(x_scrn, y_scrn)) {
            searchRoute();
            return true;
        }
        return false;
    } else {
        if (main->checkActivation(x_scrn, y_scrn)) {
            switch (main->mode) {
                case streetInSearch:
                    main->searchStreet();
                    break;

                case intersectionInSearch:
                    main->searchIntersection();
                    break;

                default:
                    ;
            }
            drawscreen();
            return true;
        }

        // if clicked on the cross button 
        if (crossBackground.intersects(x_scrn, y_scrn)) {
            clearUpSearchBars();
            return true;
        }

        if (background.intersects(x_scrn, y_scrn)) {
            // Don't move the statement below, otherwise animation wouldn't
            // be drew
            this->findRoute = true;
            backgroundStyle = predefined_styles[WHITE_100A];
            clearUpSearchBars();
            animation_manager.invoke_animation(PRESENT_SEARCH_BAR);
            return true;
        }

    }

    return false;
}

void searchBarManager::buttonMouseMoveReact(float x, float y) {
    if (background.intersects(x, y)) {
        backgroundStyle = predefined_styles[WHITE_100A];
    } else {
        backgroundStyle = predefined_styles[WHITE_50A];
    }
}

void searchBarManager::findButtonMouseMoveReact(float x, float y) {
    if (findRouteButton.intersects(x, y)) {
        findRouteButtonStyle = predefined_styles[WHITE_100A];
        findRouteButtonTransparency = 255;
    } else {
        findRouteButtonStyle = predefined_styles[WHITE_70A];
        findRouteButtonTransparency = 255 * 0.75;
    }
}

void searchBarManager::drawInstructions() {
    instructionRect test;

    float topSpaceOffSet = 5;

    setfontsize(10);

    setcolor(predefined_styles[WHITE_70A]);

    drawrect(instructionPanel);

    fillrect(instructionPanel);

    setcolor(t_color(0, 0, 0, 20));
    drawrect(t_bound_box(t_point(instructionPanel.bottom_left().x - 1, instructionPanel.bottom_left().y - 1),
            instructionPanel.get_width() + 2., instructionPanel.get_height() + 2.));

    setcolor(t_color(0, 0, 0, 10));
    drawrect(t_bound_box(t_point(instructionPanel.bottom_left().x - 2, instructionPanel.bottom_left().y - 2),
            instructionPanel.get_width() + 4., instructionPanel.get_height() + 4.));

    float text_box_init_x = instructionPanel.bottom_left().x;
    float text_box_init_y = instructionPanel.bottom_left().y + topSpaceOffSet;

    float text_box_width = instructionPanel.get_width();
    float text_box_height = 25.;


    float dividing_line_width = instructionPanel.get_width()*3 / 5;

    t_bound_box i;

    float eachInstructionHeight = 25. + 25. + 10.;

    for (unsigned index = first; index < second; index++) {
        auto eachInstruction = instructions.getOneInstructPair(index);

        // Draw first text
        i = t_bound_box(t_point(text_box_init_x, text_box_init_y + index * eachInstructionHeight), text_box_width, text_box_height);
        setcolor(predefined_styles[NULLCOLOR]);
        drawrect(i);

        setcolor(predefined_styles[BLACK_100A]);
        drawtext_in(i, eachInstruction.second.first, 100);

        // Draw second text
        i = t_bound_box(t_point(text_box_init_x, text_box_init_y + index * eachInstructionHeight + 25.), text_box_width, text_box_height);
        setcolor(predefined_styles[NULLCOLOR]);
        drawrect(i);

        setcolor(predefined_styles[BLACK_100A]);
        drawtext_in(i, eachInstruction.second.second, 100);

        if (index + 1 != second) {
            // Draw the dividing line 
            setcolor(predefined_styles[BLACK_100A]);
            drawline(t_point(instructionPanel.bottom_left().x + instructionPanel.get_width()*1 / 5, text_box_init_y + 55. + index * eachInstructionHeight),
                    t_point(instructionPanel.bottom_left().x + instructionPanel.get_width()*1 / 5 + dividing_line_width, text_box_init_y + 55. + index * eachInstructionHeight));
        }
    }
}

inline bool searchBarManager::isFindingRoute() {
    return this->findRoute;
}

searchBar* searchBarManager::inputAcceptor() {
    if (!findRoute) {
        return main;
    } else {
        if (main->activated)
            return main;
        else if (sub->activated)
            return sub;
        else
            return NULL;
    }
}

void searchBarManager::lockSearchBar(searchBar* acceptor) {
    // if search is already locked, return
    if (acceptor->mode == managerControlled)
        return;

    if (acceptor == main) {
        if (acceptor->mode != intersectionInSearch)
            return;
        if (acceptor->candidatesBox_intersection.empty())
            return;
        // Manually construct a vector from the set to allow random access
        vector<unsigned> tempIdClt(acceptor->intersectIdClt.begin(), acceptor->intersectIdClt.end());
        // Passing starting intersection to manager
        from = tempIdClt[acceptor->findSelectedAndUpdateInput()];
        // freeze main search bar
        acceptor->update();
        acceptor->mode = managerControlled;
        acceptor->activated = false;
        drawSearchBar();
    } else if (acceptor == sub) {
        if (acceptor->mode == intersectionInSearch) {
            if (acceptor->candidatesBox_intersection.empty())
                return;
            // Manually construct a vector from the set to allow random access
            vector<unsigned> tempIdClt(acceptor->intersectIdClt.begin(), acceptor->intersectIdClt.end());
            // Passing ending intersection to manager
            to = tempIdClt[acceptor->findSelectedAndUpdateInput()];
            // Set route finding mode
            searchFor = interToInter;
            acceptor->update();
            acceptor->mode = managerControlled;
            acceptor->activated = false;
        } else if (acceptor->mode == POIInSearch) {
            if (acceptor->candidatesBox_POI.empty())
                return;
            // Find which POI has been selected, pass to manager
            to_POI = acceptor->candidatesBox_POI[acceptor->findSelectedAndUpdateInput()].text;
            // set route find mode
            searchFor = interToPOI;
            acceptor->update();
            acceptor->mode = managerControlled;
            acceptor->activated = false;
        }            // Do nothing if acceptor isn't in either mode
        else
            return;
    } else {
        cerr << "Search bar passed in is neither main or sub" << endl;
        exit(-1);
    }

    acceptor->bar_color = predefined_styles[LOCKED];

}

// Reset both search bars depending on which state the manager is at right now

void searchBarManager::clearUpSearchBars() {
    inFindRoute = false;

    if (!routeFound.empty())
        routeFound.clear();

    clickPath.enable = false;
    clickPath.numClick = firstClick;

    if (findRoute) {
        this->resetSearchBar(sub);
        this->resetSearchBar(main);
    } else
        this->resetSearchBar(main);
}

static t_bound_box objVisionHelper(const vector<unsigned> & path) {
    float left = segVec[path[0]].ptsOnStreet.front().first, 
          right = segVec[path[0]].ptsOnStreet.front().first, 
          bottom = segVec[path[0]].ptsOnStreet.front().second, 
          top = segVec[path[0]].ptsOnStreet.front().second;
    
    for (const auto eachSeg: path)
    {
        for (const auto eachPt: segVec[eachSeg].ptsOnStreet)
        {
            if (eachPt.first < left)
                left = eachPt.first;
            else if (eachPt.first > right)
                right = eachPt.first;
            
            if (eachPt.second < bottom)
                bottom = eachPt.second;
            else if (eachPt.second > top)
                top = eachPt.second;
        }
    }
    
    auto retbox = t_bound_box(left + (world.left - left)*0.01 , 
                              bottom + (world.bottom - bottom)*0.01, 
                              right + (world.right - right)*0.01, 
                              top + (world.top - top)*0.01 );
    
    return retbox;
}

static coord objPointHelper(unsigned startSeg, unsigned endSeg) {
    auto start = segVec[startSeg].ptsOnStreet.front();
    auto end = segVec[endSeg].ptsOnStreet.back();
    
    auto ret = coord( (start.first + end.first)/2, (start.second + end.second)/2 );
    
    return ret;
}

void searchBarManager::searchRoute() {

    string destination;

    switch (searchFor) {
        case interToInter:
            routeFound = find_path_between_intersections(from, to, 15);
            destination = getIntersectionName(to);
            break;

        case interToPOI:
            routeFound = find_path_to_point_of_interest(from, to_POI, 15);
            destination = to_POI;
            break;
        default:
            break;
    }

    if (routeFound.empty())
        return;

    instructions = routeInstruction(routeFound, destination);

    auto objVision = objVisionHelper(routeFound);
    auto objPoint = objPointHelper(routeFound.front(), routeFound.back());
    
    animation_manager.invoke_animation(AUTO_ZOOM_OUT, objPoint, objVision);
    
    // Initilize instruction panel
    auto curScreenWidth = get_visible_screen().right();
    auto curScreenHeight = get_visible_screen().bottom();

    float backgroud_x = 10.;
    float backgroud_y = curScreenHeight * 1 / 4;

    float backgroud_width = curScreenWidth * 1 / 3;
    float backgroud_height;

    this->first = 0;

    // define height of instruction panel depending on numbers of instruction

    backgroud_height = 60 * instructions.getNumOfInstructions();
    this->second = instructions.getNumOfInstructions();

    instructions.printAllInstructions();

    instructionPanel = t_bound_box(t_point(backgroud_x, backgroud_y), backgroud_width, backgroud_height);

    inFindRoute = true;
}

inline bool searchBarManager::isBothBarLocked() {
    return (main->mode == managerControlled && sub->mode == managerControlled);
}

void searchBarManager::showUpFindButton() {
    if (findButtonOut)
        return;

    findButtonOut = true;
    findRouteButton = t_bound_box(t_point(sub->search_bar_x + sub->search_bar_width + 40., 10.), 80., 40.);

    animation_manager.invoke_animation(PRESENT_FIND_BUTTON);

}

// reset search bar from find route mode

void searchBarManager::resetSearchBar(searchBar* acceptor) {
    acceptor->mode = inactive;
    acceptor->activated = false;
    acceptor->bar_color = predefined_styles[WHITE_70A];
    acceptor->linker.clear();
    acceptor->intersectIdClt.clear();
    acceptor->inTimeInput.clear();
    acceptor->update();
}

void searchBarManager::updateSearchBarsInput(searchBar* acceptor, string text, unsigned font_color_no, unsigned bar_color_no) {
    acceptor->inTimeInput = text;

    if (font_color_no != NULLCOLOR)
        acceptor->font_color = predefined_styles[font_color_no];

    if (bar_color_no != NULLCOLOR)
        acceptor->bar_color = predefined_styles[bar_color_no];

    acceptor->update();

}

void searchBarManager::updateSearchBarsInput(searchBar* acceptor, string text, t_color font_color, t_color bar_color) {
    acceptor->inTimeInput = text;
    acceptor->font_color = font_color;
    acceptor->bar_color = bar_color;

    acceptor->update();
}

void searchBarManager::pinPlacer(unsigned interxId) {
    auto pos = itsVec[interxId].cartesian_p;
    float pos_scn_x = xworld_to_scrn(pos.first);
    float pos_scn_y = yworld_to_scrn(pos.second);

    set_coordinate_system(GL_SCREEN);

    draw_surface(surface_collection.intersectionPin, pos_scn_x - 19,
            pos_scn_y - 60);

    set_coordinate_system(GL_WORLD);
}
/********************** Implementations of searchBar class *********************/
// Constructor for search Bar

searchBar::searchBarStruct(double _x, double _y) :
inTimeInput(""), POI_prefix("POI:"), search_bar_x(_x), search_bar_y(_y),
search_bar_width(300.), search_bar_height(40.), transparency(20),
text_x_center(search_bar_x + search_bar_width / 2 + 5),
text_y_center((search_bar_y + search_bar_height) / 2 + 5),
activated(false), orginalWidth(search_bar_width),
candidateBoxHeight(40.), widthExtension(0.), mode(inactive) {
    bar_color = t_color(255, 255, 255, 178);
}

void searchBar::update() {
    // Dealing with search bar width corresponding to 
    if (this->inTimeInput.length() > 20) {
        this->search_bar_width = this->orginalWidth + (this->inTimeInput.length() - 20) * 2;
        this->text_x_center = this->orginalWidth / 2 + this->search_bar_x + (this->inTimeInput.length() - 20) * 1;
    } else {
        this->search_bar_width = this->orginalWidth;
        this->text_x_center = this->orginalWidth / 2 + this->search_bar_x;
    }

    switch (this->mode) {
        case streetInSearch:
            this->updateCandidates_street();

            this->secondStreet.clear();

            break;

        case intersectionInSearch:
            this->updateCandidates_intersection();

            break;

        case POIInSearch:
            this->updateCandidates_POI();

            break;

        default:
            ;
    }
}

void searchBar::updateCandidates_street() {

    this->activeSearch->update(this->inTimeInput);

    auto candidates = this->activeSearch->getCandidates();

    this->candidatesBox_street.clear();

    // reset width extension for new candidates group
    this->widthExtension = 0;

    // Determine the width of candidates box in terms of the longest length candidate 
    for (auto iter = candidates.begin(); iter != candidates.end(); iter++) {
        if (iter->length() > 15) {
            float selfExt = (iter->length() - 15) * 7;
            if (selfExt > widthExtension)
                widthExtension = selfExt;
        }
    }

    for (size_t index = 0; index < candidates.size(); index++) {
        candidateRect candidateTemp(candidates[index], index, this);
        this->candidatesBox_street.push_back(candidateTemp);
    }
}

void searchBar::updateCandidates_intersection() {

    this->anotherActiveSearch->update(this->secondStreet);

    auto candidates = this->anotherActiveSearch->getCandidates();

    this->candidatesBox_intersection.clear();

    this->intersectIdClt.clear();

    this->widthExtension = 0;

    vector<string> intersectionClt;

    for (auto iter = candidates.begin(); iter != candidates.end(); iter++) {
        auto eachSetOfCandidate = find_intersection_ids_from_street_names(stringParser(this->firstStreet), stringParser(*iter));
        intersectIdClt.insert(eachSetOfCandidate.begin(), eachSetOfCandidate.end());
    }

    for (auto iter = intersectIdClt.begin(); iter != intersectIdClt.end(); iter++) {
        string intersectionName = getIntersectionName(*iter);

        if (intersectionName.length() > 15) {
            float selfExt = (intersectionName.length() - 15) * 7;
            if (selfExt > widthExtension)
                widthExtension = selfExt;
        }

        intersectionClt.push_back(intersectionName);
    }

    for (size_t index = 0; index < intersectionClt.size(); index++) {
        candidateRect candidateTemp(intersectionClt[index], index, this);

        this->candidatesBox_intersection.push_back(candidateTemp);
    }

}

void searchBar::updateCandidates_POI() {
    this->activeSearch_POI->update(POIHandler());

    auto candidates = this->activeSearch_POI->getCandidates();

    this->candidatesBox_POI.clear();

    // reset width extension for new candidates group
    this->widthExtension = 0;

    // Determine the width of candidates box in terms of the longest length candidate 
    for (auto iter = candidates.begin(); iter != candidates.end(); iter++) {
        if (iter->length() > 15) {
            float selfExt = (iter->length() - 15) * 7;
            if (selfExt > widthExtension)
                widthExtension = selfExt;
        }
    }

    for (size_t index = 0; index < candidates.size(); index++) {
        candidateRect candidateTemp(candidates[index], index, this);
        this->candidatesBox_POI.push_back(candidateTemp);
    }
}

void searchBar::updateCandidatesStyle() {
    switch (this->mode) {
        case streetInSearch:
            for (auto iter = this->candidatesBox_street.begin(); iter != this->candidatesBox_street.end();
                    iter++) {
                (*iter).switcher(iter->selected);
            }
            break;

        case intersectionInSearch:
            for (auto iter = this->candidatesBox_intersection.begin(); iter != this->candidatesBox_intersection.end();
                    iter++) {
                (*iter).switcher(iter->selected);
            }
            break;

        case POIInSearch:
            for (auto iter = this->candidatesBox_POI.begin(); iter != this->candidatesBox_POI.end();
                    iter++) {
                (*iter).switcher(iter->selected);
            }
            break;

        default:
            ;
    }
}

void searchBar::updateCandidatesStyle(float x, float y) {
    switch (this->mode) {
        case streetInSearch:
            for (auto iter = this->candidatesBox_street.begin(); iter != this->candidatesBox_street.end();
                    iter++) {
                (*iter).update(x, y);
            }
            break;

        case intersectionInSearch:
            for (auto iter = this->candidatesBox_intersection.begin(); iter != this->candidatesBox_intersection.end();
                    iter++) {
                (*iter).update(x, y);
            }
            break;

        case POIInSearch:
            for (auto iter = this->candidatesBox_POI.begin(); iter != this->candidatesBox_POI.end();
                    iter++) {
                (*iter).update(x, y);
            }
            break;

        default:;
    }
}

bool searchBar::checkActivation(float x_scrn, float y_scrn) {

    if (mode != managerControlled) {
        if (this->search_bar_x <= x_scrn && x_scrn <= (this->search_bar_width + this->search_bar_x) &&
                this->search_bar_y <= y_scrn && y_scrn <= (this->search_bar_height + this->search_bar_y)) {
            this->activated = true;
            this->bar_color = predefined_styles[WHITE_100A];
            return this->activated;
        } else if (this->checkOnSearchSection()) {
            this->activated = false;
            this->bar_color = predefined_styles[WHITE_70A];
            return true;
        } else {
            this->activated = false;
            this->mode = 0;
            this->bar_color = predefined_styles[WHITE_70A];
            return this->activated;
        }
    } else {
        if (this->search_bar_x <= x_scrn && x_scrn <= (this->search_bar_width + this->search_bar_x) &&
                this->search_bar_y <= y_scrn && y_scrn <= (this->search_bar_height + this->search_bar_y)) {
            return true;
        } else {
            this->activated = false;
            return this->activated;
        }
    }
}

void searchBar::input(char key_pressed) {
    if (32 <= static_cast<unsigned> (key_pressed)) {
        this->inTimeInput += key_pressed;
    }

    switch (this->mode) {

        case inactive:
            // Check for timing of search
            if (this->inTimeInput.length() > 1 && this->inTimeInput.back() == ' ') {
                if (!checkForPOIPrefix()) {
                    this->mode = streetInSearch;
                    this->activateSearch();
                } else {
                    this->mode = waitForInput_POI;
                }
            }
            break;

        case streetInSearch:
            // advance search state
            if (this->isSecondStreetInput()) {
                this->mode = waitForInput;
                this->streetsHandler();
            }
            break;

        case waitForInput:

            if (32 <= static_cast<unsigned> (key_pressed)) {
                this->secondStreet = getSecondStreetFromInTimeInput();
            }

            if (this->secondStreet.length() > 1 && this->secondStreet.back() == ' ') {
                this->mode = intersectionInSearch;
                this->activateIntersectionSearch();
            }

            break;

        case intersectionInSearch:
            this->secondStreet = getSecondStreetFromInTimeInput();
            break;

        case waitForInput_POI:
            if (POIHandler().size() > 1 && POIHandler().back() == ' ') {
                this->mode = POIInSearch;
                this->activateSearch(true);
            }

            break;

        default:;
    }

    this->update();
}

void searchBar::backspace() {

    if (this->inTimeInput.length() != 0)
        this->inTimeInput = string(this->inTimeInput.begin(), this->inTimeInput.end() - 1);

    switch (this->mode) {
        case streetInSearch:
            // Do not satisfy mode proceed conditions, dealt as street in search state
            if (!this->isSecondStreetInput()) {
                // Backspace to space, triggers research
                if (this->inTimeInput.length() > 1 && this->inTimeInput.back() == ' ') {
                    delete this->activeSearch;
                    this->activateSearch();
                }// otherwise the input is empty, put search bar back to inactive
                else if (this->inTimeInput.empty()) {
                    this->candidatesBox_street.clear();
                    this->mode = inactive;
                }
            } else
                this->mode = waitForInput;

            this->update();

            break;

        case highlightStreet:
            this->mode = streetInSearch;
            break;

        case waitForInput:
            if (this->secondStreet.empty()) {
                if (!this->isSecondStreetInput()) {
                    this->mode = streetInSearch;
                }
            }

            break;

        case intersectionInSearch:
            // Update second street
            if (!this->secondStreet.empty())
                this->secondStreet = string(this->secondStreet.begin(), this->secondStreet.end() - 1);

            // Update state in terms of the second street
            if (this->secondStreet.length() > 1 && this->secondStreet.back() == ' ') {
                delete this->anotherActiveSearch;
                this->activateIntersectionSearch();
            }// 
            else if (this->secondStreet.empty()) {
                this->intersectIdClt.clear();
                this->candidatesBox_intersection.clear();
                this->mode = waitForInput;
            }

            this->update();

            break;

        case highlightIntersection:
            this->mode = intersectionInSearch;
            break;

        case waitForInput_POI:
            if (!checkForPOIPrefix())
                this->mode = inactive;
            break;

        case POIInSearch:
            if (POIHandler().length() > 1 && this->inTimeInput.back() == ' ') {
                delete activeSearch_POI;
                this->activateSearch(true);
            } else if (POIHandler().empty()) {
                this->candidatesBox_POI.clear();
                this->mode = waitForInput_POI;
            }

            this->update();

            break;

        case highlightPOI:
            this->mode = POIInSearch;
            break;

        default:;
    }
}

void searchBar::activateSearch(bool searchPOI) {
    if (!searchPOI)
        this->activeSearch = new FuzzySearch(this->inTimeInput);
    else
        this->activeSearch_POI = new FuzzySearch_POI(POIHandler());
}

void searchBar::activateIntersectionSearch() {
    this->anotherActiveSearch = new FuzzySearch(this->secondStreet);
}

static coord streetResultAnimeHelper() {
    if (!streetResult.empty()) {
        auto headLatLon = segVec[streetResult.front()].ptsOnStreet.front();
        auto tailLatLon = segVec[streetResult.back()].ptsOnStreet.back();

        auto retLatLon = coord((headLatLon.first + tailLatLon.first) / 2,
                (headLatLon.second + tailLatLon.second) / 2);

        return retLatLon;
    } else {
        cerr << "Street result vector is empty" << endl;
        exit(-1);
    }
}

void searchBar::searchStreet() {

    if (candidatesBox_street.empty())
        return;

    // Clear out previous data
    if (streetResult.size() != 0)
        streetResult.clear();

    string object = candidatesBox_street[findSelectedAndUpdateInput()].text;

    this->mode = highlightStreet;

    auto searchResult = searchStreetId(object);

    for (const auto &eachStreet : searchResult) {
        auto &segClt = stVec[eachStreet].StreetSegVec;

        streetResult.insert(streetResult.end(), segClt.begin(), segClt.end());
    }

    auto animeCoord = streetResultAnimeHelper();

    animation_manager.invoke_animation(AUTO_ZOOM_OUT, animeCoord);
}

void searchBar::searchIntersection() {

    if (candidatesBox_intersection.empty())
        return;

    if (!intersectionResult.empty())
        intersectionResult.clear();

    unsigned index = findSelectedAndUpdateInput();

    vector<unsigned> tempInterxIdClt(intersectIdClt.begin(), intersectIdClt.end());

    this->mode = highlightIntersection;

    auto curIntersection = (*getPtrToIntersectSet())[ tempInterxIdClt[index] ];

    auto itsStruct = intersectionPin(curIntersection.cartesian_p.first, curIntersection.cartesian_p.second);

    intersectionResult.push_back(itsStruct);

    auto animeCoord = coord(intersectionResult[0].x, intersectionResult[0].y);

    animation_manager.invoke_animation(AUTO_ZOOM_OUT, animeCoord);
}

void searchBar::searchPOIs() {

    if (candidatesBox_POI.empty())
        return;

    if (!POIResult.empty())
        POIResult.clear();

    string object = candidatesBox_POI[findSelectedAndUpdateInput()].text;

    this->mode = highlightPOI;

    auto POIs = (*getPtrToSearchLibPOI_map()).at(object);

    for (auto rawPOI : POIs) {
        auto temp = POIPin(rawPOI.POIcoord.first, rawPOI.POIcoord.second);

        POIResult.push_back(temp);
    }

}

// In the case where mouse click is detected, check if mouse has clicked on one 
// of the candidate boxes

bool searchBar::checkOnSearchSection() {
    switch (this->mode) {
        case streetInSearch:
            for (auto iter = this->candidatesBox_street.begin(); iter != this->candidatesBox_street.end(); iter++) {
                if (iter->selected)
                    return true;
            }
            break;

        case intersectionInSearch:
            for (auto iter = this->candidatesBox_intersection.begin(); iter != this->candidatesBox_intersection.end(); iter++) {
                if (iter->selected)
                    return true;
            }
            break;

        default:;
    }
    return false;
}

// Clear highlights on the screen

void searchBar::clearHighlights() {

    this->mode = streetInSearch;

    if (!streetResult.empty())
        streetResult.clear();

    clearCandidatesFlag();
}

// Clear pins placed on the screen

void searchBar::clearPins() {
    this->mode = intersectionInSearch;

    if (!intersectionResult.empty())
        intersectionResult.clear();

    clearCandidatesFlag();
}

// Clear out "selected" attribute for each candidates already exists in vectors

void searchBar::clearCandidatesFlag() {
    for (auto iter = this->candidatesBox_street.begin(); iter != this->candidatesBox_street.end(); iter++) {
        if (iter->selected)
            iter->selected = false;
    }
    for (auto iter = this->candidatesBox_intersection.begin(); iter != this->candidatesBox_intersection.end(); iter++) {
        if (iter->selected)
            iter->selected = false;
    }
}

void searchBar::autoComplete() {

    string candidate, comWord, temp;

    
    switch (this->mode) {
        case streetInSearch:
            if (candidatesBox_street.empty())
                return;
            for (auto iter = this->candidatesBox_street.begin(); iter != this->candidatesBox_street.end(); iter++) {
                if (iter->selected)
                    candidate = iter->text;
            }
            break;

        case intersectionInSearch:
            if (candidatesBox_intersection.empty())
                return;
            for (auto iter = this->candidatesBox_intersection.begin(); iter != this->candidatesBox_intersection.end(); iter++) {
                if (iter->selected)
                    candidate = iter->text;
            }
            break;

        default:
            return;
    }

    if (candidate == this->inTimeInput)
        return;

    stringstream candidateStream(candidate);
    
    stringstream inputStream(this->inTimeInput);
    
    vector<string> inputSlice;
    vector<string> candidateSlice;

    string tempResult = "";
    
    while (candidateStream >> temp)
    {
        candidateSlice.push_back(temp);
    }
    
    while (inputStream >> temp)
    {
        inputSlice.push_back(temp);
    }
    
    if (inputSlice.size() > candidateSlice.size())
    {
        for (const auto & eachWord : candidateSlice)
        {
            tempResult += eachWord;
            tempResult += ' ';
        }
    }
    else if (stringParser(inputSlice.back()) == stringParser(candidateSlice[inputSlice.size() - 1]))
    {
        for (unsigned index = 0; index < inputSlice.size() + 1; index++ )
        {
            tempResult += candidateSlice[index];
            tempResult += ' ';
        }
    }
    else
    {
        for (unsigned index = 0; index < inputSlice.size(); index++ )
        {
            tempResult += candidateSlice[index];
            tempResult += ' ';
        }
    }
    
    this->inTimeInput = tempResult;

    this->activeSearch->update(this->inTimeInput);

    this->update();

}

bool searchBar::isSecondStreetInput() {

    if (candidatesBox_street.empty())
        return false;

    unsigned inputWordCount = 0, candidateWordCount;

    string lastWord;

    stringstream inputStream(this->inTimeInput);

    while (inputStream >> lastWord) {
        inputWordCount++;
    }

    candidateWordCount = wordCounter(candidatesBox_street[0].text);

    if ((candidateWordCount == inputWordCount - 1) &&
            (find(streetLinker.begin(), streetLinker.end(), lastWord) != streetLinker.end())) {
        return true;
    } else
        return false;

}

// This function helps break the in time input down to the first street name
// and the linking word

void searchBar::streetsHandler() {

    if (candidatesBox_street.empty())
        return;

    unsigned candidateWordCount = wordCounter(this->candidatesBox_street[0].text);

    unsigned counter = 0;

    string currentWord;

    stringstream tempStream(this->inTimeInput);

    this->firstStreet.clear();

    while (counter < candidateWordCount) {
        tempStream >> currentWord;
        counter++;
    }

    this->firstStreet = this->candidatesBox_street[0].text;

    // Acquire the linking word
    if (tempStream >> currentWord)
        linker = currentWord;
    else {
        cerr << "Cannot locate linking word, exiting" << endl;
        exit(-1);
    }
}

inline string searchBar::POIHandler() {
    if (inTimeInput.begin() + POI_prefix.size() < inTimeInput.end())
        return string(inTimeInput.begin() + POI_prefix.size() + 1, inTimeInput.end());
    else
        return "";
}

inline string searchBar::getSecondStreetFromInTimeInput() {
    if (inTimeInput.find(linker) + linker.size() >= inTimeInput.size())
        return "";
    else
        return inTimeInput.substr(inTimeInput.find(linker) + linker.size());
}

// return index of candidates list of which has been selected

unsigned searchBar::findSelectedAndUpdateInput() {
    vector<candidateRect>* selectedType;

    switch (mode) {
        case streetInSearch:
            selectedType = &candidatesBox_street;
            break;

        case intersectionInSearch:
            selectedType = &candidatesBox_intersection;
            break;

        case POIInSearch:
            selectedType = &candidatesBox_POI;
            break;

        default:
            cout << "Detected mode: " << mode << endl;
            cerr << "Search bar mode is not valid, exiting" << endl;
            exit(-1);
    }

    for (const auto &candidate : (*selectedType)) {
        if (candidate.selected) {
            this->inTimeInput = candidate.text;
            return candidate.index;
        }
    }
    return selectedType->size();
}

bool searchBar::checkForPOIPrefix() {
    string prefix;

    // Get the first word
    if (inTimeInput.size() >= POI_prefix.size())
        prefix = string(inTimeInput.begin(), inTimeInput.begin() + POI_prefix.size());
    else
        return false;

    boost::to_upper(prefix);

    if (prefix == POI_prefix)
        return true;
    else
        return false;
}

void searchBar::shiftUpSelection() {

    std::vector<candidateRect>* type;
    bool correctMode = false;

    switch (this->mode) {
        case streetInSearch:
            type = &candidatesBox_street;
            correctMode = true;
            break;

        case intersectionInSearch:
            type = &candidatesBox_intersection;
            correctMode = true;
            break;

        case POIInSearch:
            type = &candidatesBox_POI;
            correctMode = true;
            break;

        default:
            ;
    }

    if (correctMode)
        for (unsigned index = 0; index < (*type).size(); index++) {
            if ((*type)[index].selected) {
                if (index == 0)
                    (*type)[(*type).size() - 1].selected = true;
                else
                    (*type)[index - 1].selected = true;

                (*type)[index].selected = false;

                break;
            }
        }
}

void searchBar::shiftDownSelection() {

    std::vector<candidateRect>* type;

    bool correctMode = false;

    switch (this->mode) {
        case streetInSearch:
            type = &candidatesBox_street;
            correctMode = true;
            break;

        case intersectionInSearch:
            type = &candidatesBox_intersection;
            correctMode = true;
            break;

        case POIInSearch:
            type = &candidatesBox_POI;
            correctMode = true;
            break;

        default:
            ;
    }

    if (correctMode)
        for (unsigned index = 0; index < (*type).size(); index++) {
            if ((*type)[index].selected) {
                if (index + 1 == (*type).size())
                    (*type)[0].selected = true;
                else
                    (*type)[index + 1].selected = true;

                (*type)[index].selected = false;

                break;
            }
        }
}

void searchBar::drawself() {
    set_coordinate_system(GL_SCREEN);

    setlinestyle(SOLID, BUTT);
    setlinewidth(2);

    auto colorBefor = getcolor();

    setcolor(t_color(0, 0, 0, transparency));

    drawrect(search_bar_x - 1, search_bar_y - 1,
            search_bar_x + search_bar_width + 2, search_bar_y + search_bar_height + 2);

    setcolor(t_color(0, 0, 0, transparency / 2));

    drawrect(search_bar_x - 2, search_bar_y - 2,
            search_bar_x + search_bar_width + 2, search_bar_y + search_bar_height + 2);

    setcolor(bar_color);

    drawrect(search_bar_x, search_bar_y,
            search_bar_x + search_bar_width, search_bar_y + search_bar_height);

    fillrect(search_bar_x, search_bar_y,
            search_bar_x + search_bar_width, search_bar_y + search_bar_height);

    setcolor(font_color);

    drawtext(text_x_center, text_y_center, inTimeInput);

    if (activated) {
        switch (mode) {
            case streetInSearch:
                drawCandidates(candidatesBox_street);
                break;

            case intersectionInSearch:
                drawCandidates(candidatesBox_intersection);
                break;

            case POIInSearch:
                drawCandidates(candidatesBox_POI);
                break;

            default:;
        }
    }

    setcolor(colorBefor);

    set_coordinate_system(GL_WORLD);
}

/****************** Implementation of structure candidateRect ******************/
candidateRect::candidateRect(std::string _text, unsigned order, searchBarStruct* motherBar) :
ori_hegiht_bar(motherBar->search_bar_y + 8), text(stringParser(_text)),
rect(t_bound_box(t_point(motherBar->search_bar_x,
ori_hegiht_bar + motherBar->search_bar_height +
motherBar->candidateBoxHeight*(order)),
motherBar->orginalWidth + motherBar->widthExtension,
motherBar->candidateBoxHeight)), index(order) {
    this->switcher((order == 0));
}

void candidateRect::update(float x, float y) {

    this->switcher(this->rect.intersects(x, y));

}

inline void candidateRect::switcher(bool active) {
    if (active) {
        this->rect_style = predefined_styles[WHITE_100A];
        this->text_style = predefined_styles[BLACK_100A];
        this->selected = true;
    } else {
        this->rect_style = predefined_styles[WHITE_50A];
        this->text_style = predefined_styles[BLACK_50A];
        this->selected = false;
    }
}

/******************** Implementation of animation manager **********************/

animationManager::animationManager(searchBar* _obj) :
anime_obj(_obj), in_animation(false), current_anime(0),
current_frame(0), total_frame(0) {
}

void animationManager::invoke_animation(unsigned ani_no, coord obj, t_bound_box vision) {

    objVision = vision;

    switch (ani_no) {
        case PRESENT_SEARCH_BAR:
            current_anime = ani_no;
            total_frame = 3;
            break;

        case WARP_UP_SEARCH_BAR:
            current_anime = ani_no;
            total_frame = 3;
            break;

        case AUTO_ZOOM_OUT:
            current_anime = ani_no;
            total_frame = 30;
            this->obj_x = obj.first;
            this->obj_y = obj.second;
            break;

        case PRESENT_FIND_BUTTON:
            current_anime = ani_no;
            total_frame = 3;
            break;

        case WARP_UP_FIND_BUTTON:
            current_anime = ani_no;
            total_frame = 3;
            break;

        default:
            return;
    }

    in_animation = true;

    play();

    in_animation = false;
}

inline bool animationManager::isInAnimation() {
    if (current_anime == AUTO_ZOOM_OUT)
        return this->in_animation;
    else
        return false;
}

// Invoked at the initialization, determine configurations of the animation

void animationManager::zooming_value_detector() {

    set_coordinate_system(GL_SCREEN);

    t_bound_box curWorld = get_visible_world();

    t_point cur_center = curWorld.get_center();

    t_point relay_center((cur_center.x + obj_x) / 2, (cur_center.y + obj_y) / 2);

    double factor = 0.1;

    double left_diff = abs(world.left - curWorld.left()) * factor;
    double bottom_diff = abs(world.bottom - curWorld.bottom()) * factor;
    double right_diff = abs(world.right - curWorld.right()) * factor;
    double top_diff = abs(world.top - curWorld.top()) * factor;

    zoom_out_confi.left = curWorld.left() + (relay_center.x - cur_center.x) - left_diff;
    zoom_out_confi.bottom = curWorld.bottom() + (relay_center.y - cur_center.y) - bottom_diff;
    zoom_out_confi.right = curWorld.right() + (relay_center.x - cur_center.x) + right_diff;
    zoom_out_confi.top = curWorld.top() + (relay_center.y - cur_center.y) + top_diff;

}

void animationManager::search_bar_in() {
    // value of transparency of 20 of the edge of the search bar
    float edge_step_value = 20 / total_frame;
    // value of transparency of 178 of the search bar
    float step_value = 178 / total_frame;

    while (current_frame <= total_frame) {
        anime_obj->transparency = edge_step_value*current_frame;

        anime_obj->bar_color = t_color(255, 255, 255, step_value * current_frame);

        current_frame++;

        drawscreen();
    }
}

void animationManager::search_bar_out() {
    // value of transparency of 20 of the edge of the search bar
    float edge_step_value = 20 / total_frame;
    // value of transparency of 178 of the search bar
    float step_value = 178 / total_frame;

    while (current_frame <= total_frame) {
        anime_obj->transparency = 20 - edge_step_value*current_frame;

        anime_obj->bar_color = t_color(255, 255, 255, 178 - step_value * current_frame);

        current_frame++;

        drawscreen();
    }
}

void animationManager::delay(long milliseconds) {
    std::chrono::milliseconds duration(milliseconds);
    std::this_thread::sleep_for(duration);
}

void animationManager::setObjConfiguration() {
    if (objVision.left() != 0 &&
            objVision.right() != 0 &&
            objVision.top() != 0 &&
            objVision.bottom() != 0) {
        cout << "Here" << endl;
        objConfi.left = objVision.left();
        objConfi.bottom = objVision.bottom();
        objConfi.right = objVision.right();
        objConfi.top = objVision.top();
    } else {
        float zoomScale_out = .01;

        float screenOffSetY = zoomScale_out*screen_ratio_height;
        float screenOffSetX = zoomScale_out*screen_ratio_width;


        objConfi.top = obj_y + screenOffSetY / 2;
        objConfi.bottom = obj_y - screenOffSetY / 2;
        objConfi.left = obj_x - screenOffSetX / 2;
        objConfi.right = obj_x + screenOffSetX / 2;
    }
}

void animationManager::zoom_out_and_in() {

    zooming_value_detector();

    // Initialize current coord information
    float curLeft = get_visible_world().left();
    float curRight = get_visible_world().right();
    float curTop = get_visible_world().top();
    float curBottom = get_visible_world().bottom();
    
    printf("c l:%f b:%f r:%f t:%f", curLeft, curRight, curTop, curBottom);
    
    setObjConfiguration();

    float left_step = (zoom_out_confi.left - curLeft) / total_frame;
    float right_step = (zoom_out_confi.right - curRight) / total_frame;
    float top_step = (zoom_out_confi.top - curTop) / total_frame;
    float bottom_step = (zoom_out_confi.bottom - curBottom) / total_frame;

    
    while (current_frame <= total_frame) {
        set_visible_world(curLeft + left_step*current_frame,
                curBottom + bottom_step*current_frame,
                curRight + right_step*current_frame,
                curTop + top_step * current_frame);

        current_frame++;
        drawscreen();
    }

    current_frame = 1;

    curLeft = get_visible_world().left();
    curRight = get_visible_world().right();
    curTop = get_visible_world().top();
    curBottom = get_visible_world().bottom();


    left_step = (curLeft - objConfi.left) / total_frame;
    right_step = (curRight - objConfi.right) / total_frame;
    top_step = (curTop - objConfi.top) / total_frame;
    bottom_step = (curBottom - objConfi.bottom) / total_frame;


    while (current_frame <= total_frame) {
        set_visible_world(curLeft - left_step*current_frame,
                curBottom - bottom_step*current_frame,
                curRight - right_step*current_frame,
                curTop - top_step * current_frame);


        current_frame++;
        drawscreen();
    }

}

void animationManager::find_button_in() {
    // value of transparency of 178 of the search bar
    float step_value = 178 / total_frame;

    while (current_frame <= total_frame) {
        search_bar_manager.findRouteButtonTransparency = static_cast<unsigned> (step_value*current_frame);

        search_bar_manager.findRouteButtonStyle = t_color(255, 255, 255, search_bar_manager.findRouteButtonTransparency);

        current_frame++;

        drawscreen();
    }
}

void animationManager::find_button_out() {

}

void animationManager::play() {
    switch (current_anime) {
        case PRESENT_SEARCH_BAR:
            search_bar_in();
            break;

        case WARP_UP_SEARCH_BAR:
            search_bar_out();
            break;

        case AUTO_ZOOM_OUT:
            zoom_out_and_in();
            break;

        case PRESENT_FIND_BUTTON:
            find_button_in();

        case WARP_UP_FIND_BUTTON:
            find_button_out();

        default:
            ;
    }
    // animation finished playing, reset animation manager
    if (current_frame > total_frame) {
        in_animation = false;
        total_frame = 0;
        current_frame = 0;

        return;
    }
}

/**************** Implementation of structure pins ****************/
intersectionPin::intersectionPin(float _x, float _y) :
x(_x), y(_y) {
}

POIPin::POIPin(float _x, float _y) :
x(_x), y(_y) {
}

/******************** helper functions to draw_poi ****************************/
void react_to_landing_poi(float x, float y) {

    if (existPin == false) {

        float curLat = y;
        float curLon = x / (cos(getIntersectionPosition(0).lat() * DEG_TO_RAD));
        string output = "Lat:";
        output.append(to_string(curLat));
        output.append("\nLon: ");
        output.append(to_string(curLon));

        float x_screen = (xworld_to_scrn(x));
        float y_screen = yworld_to_scrn(y);
        landing_poi.x = x;
        landing_poi.y = y;
        landing_poi.info = output;
        landing_poi.draw = true;
        set_coordinate_system(GL_SCREEN);

        draw_surface(surface_collection.poi, x_screen - 10, y_screen - 20);
        t_bound_box box(x_screen - 100, y_screen, x_screen + 100, y_screen + 20);
        setfontsize(10);
        setcolor(BLACK);
        drawtext_in(box, output);
        setfontsize(10);
        set_coordinate_system(GL_WORLD);

        existPin = true;

    } else {
        landing_poi.draw = false;
        drawscreen();
        existPin = false;
        react_to_landing_poi(x, y);
    }
}

void highlightStreets(const vector<unsigned>& streetIdxClt, float zooming_level, unsigned color) {

    float scale_factor;

    for (const auto &eachSeg : streetIdxClt) {
        // setting line width and color 
        string seg_level = segVec[eachSeg].level;

        scale_factor = getScaleFactor(zooming_level, seg_level);

        setlinewidth((scale_factor / zoom_level) + 5);

        setcolor(predefined_styles[color]);


        double startx, starty, endx, endy;

        auto &ptsClt = segVec[eachSeg].ptsOnStreet;

        for (unsigned index = 1; index < ptsClt.size(); index++) {
            // First point
            startx = ptsClt[index - 1].first;
            starty = ptsClt[index - 1].second;
            // next point
            endx = ptsClt[index].first;
            endy = ptsClt[index].second;
            // Connect two points together
            drawline(startx, starty, endx, endy);
        }
    }

}

/********************* Call back functions for buttons *************************/
void (button_func_uni_off) (void (drawscreen) (void)) {
    //if pressed button and button is on
    if (all_buttons.uniON) {
        //turn it off
        all_buttons.uniON = false;
        change_button_text("University Off", "University On");
        //change draw to not draw
        for (auto iter = getPtrToPOISet()->begin(); iter != getPtrToPOISet()->end(); iter++) {
            if (iter->type == "university")
                iter->draw = false;
        }

    } else {
        //turn it on
        all_buttons.uniON = true;
        change_button_text("University On", "University Off");
        for (auto iter = getPtrToPOISet()->begin(); iter != getPtrToPOISet()->end(); iter++) {
            if (iter->type == "university")
                iter->draw = true;
        }
    }
    drawscreen();
}

void (button_func_hospital_off) (void (drawscreen) (void)) {
    //if pressed button and button is on
    if (all_buttons.hospitalOn) {
        //turn it off
        all_buttons.hospitalOn = false;
        change_button_text("Hospital Off", "Hospital On");
        //change draw to not draw
        for (auto iter = getPtrToPOISet()->begin(); iter != getPtrToPOISet()->end(); iter++) {
            if (iter->type == "hospital")
                iter->draw = false;
        }
    } else {
        //turn it on
        all_buttons.hospitalOn = true;
        change_button_text("Hospital On", "Hospital Off");
        for (auto iter = getPtrToPOISet()->begin(); iter != getPtrToPOISet()->end(); iter++) {
            if (iter->type == "hospital")
                iter->draw = true;

        }
    }
    drawscreen();
}

void (button_func_bank_off) (void (drawscreen) (void)) {
    //if pressed button and button is on
    if (all_buttons.bankOn) {
        //turn it off
        all_buttons.bankOn = false;
        change_button_text("Bank Off", "Bank On");
        //change draw to not draw
        for (auto iter = getPtrToPOISet()->begin(); iter != getPtrToPOISet()->end(); iter++) {
            if (iter->type == "bank")
                iter->draw = false;
        }
    } else {
        //turn it on
        all_buttons.bankOn = true;
        change_button_text("Bank On", "Bank Off");
        for (auto iter = getPtrToPOISet()->begin(); iter != getPtrToPOISet()->end(); iter++) {
            if (iter->type == "bank")
                iter->draw = true;

        }
    }
    drawscreen();
}

void (button_func_food_off) (void (drawscreen) (void)) {
    //if pressed button and button is on
    if (all_buttons.foodOn) {
        //turn it off
        all_buttons.foodOn = false;
        change_button_text("Food Off", "Food On");
        //change draw to not draw
        for (auto iter = getPtrToPOISet()->begin(); iter != getPtrToPOISet()->end(); iter++) {
            if (iter->type == "fast_food" || iter->type == "restaurant")
                iter->draw = false;
        }
    } else {
        //turn it on
        all_buttons.foodOn = true;
        change_button_text("Food On", "Food Off");
        for (auto iter = getPtrToPOISet()->begin(); iter != getPtrToPOISet()->end(); iter++) {
            if (iter->type == "fast_food" || iter->type == "restaurant")
                iter->draw = true;

        }
    }
    drawscreen();
}

void (button_func_gas_off) (void (drawscreen) (void)) {
    //if pressed button and button is on
    if (all_buttons.petroOn) {
        //turn it off
        all_buttons.petroOn = false;
        change_button_text("Petro Of", "Petro On");
        //change draw to not draw
        for (auto iter = getPtrToPOISet()->begin(); iter != getPtrToPOISet()->end(); iter++) {
            if (iter->type == "fuel")
                iter->draw = false;
        }
    } else {
        //turn it on
        all_buttons.petroOn = true;
        change_button_text("Petro On", "Petro Off");
        for (auto iter = getPtrToPOISet()->begin(); iter != getPtrToPOISet()->end(); iter++) {
            if (iter->type == "fuel")
                iter->draw = true;

        }
    }
    drawscreen();
}

void (button_func_parking_off) (void (drawscreen) (void)) {
    //if pressed button and button is on
    if (all_buttons.parkOn) {
        //turn it off
        all_buttons.parkOn = false;
        change_button_text("Parking Off", "Parking On");
        //change draw to not draw
        for (auto iter = getPtrToPOISet()->begin(); iter != getPtrToPOISet()->end(); iter++) {
            if (iter->type == "parking")
                iter->draw = false;

        }
    } else {
        //turn it on
        all_buttons.parkOn = true;
        change_button_text("Parking On", "Parking Off");
        for (auto iter = getPtrToPOISet()->begin(); iter != getPtrToPOISet()->end(); iter++) {
            if (iter->type == "parking")
                iter->draw = true;

        }
    }
    drawscreen();
}

void (button_func_all_off) (void (drawscreen) (void)) {
    if (all_buttons.allOn) {
        //turn it off
        all_buttons.allOn = false;
        change_button_text("All POI Off", "All POI On");
        //change draw to not draw
        for (auto iter = getPtrToPOISet()->begin(); iter != getPtrToPOISet()->end(); iter++) {

            iter->draw = false;
        }
    } else {
        //turn it on
        all_buttons.allOn = true;
        change_button_text("All POI On", "All POI Off");
        for (auto iter = getPtrToPOISet()->begin(); iter != getPtrToPOISet()->end(); iter++) {

            iter->draw = true;

        }
    }

    drawscreen();
}

void button_func_query(void (drawscreen) (void)) {
    search_bar_manager.clickPath.enable = true;
    
    update_message("Query Path on, please click on 2 intersections on the map");
    
    if (!search_bar_manager.findRoute)
    {
        search_bar_manager.findRoute = true;

        animation_manager.invoke_animation(PRESENT_SEARCH_BAR);
    }

    drawscreen();
}

void button_func_help(void (drawscreen) (void)) {
    if (!draw_help){

        draw_help = true;
    }
    else{
        draw_help = false;

    }
    drawscreen();
}

void highlight_intersection() {

    unsigned num_segs = getIntersectionStreetSegmentCount(intersectionToHighlight);

    for (unsigned i = 0; i < num_segs; i++) {

        StreetSegClt cur_seg = (*getPtrToStSegSet())[ getIntersectionStreetSegment(intersectionToHighlight, i)];

        //drawing segments by curve points
        for (auto iter_street = cur_seg.ptsOnStreet.begin();
                (iter_street + 1) != cur_seg.ptsOnStreet.end();
                iter_street++) {

            double startx, starty, endx, endy;

            startx = (*iter_street).first;
            starty = (*iter_street).second;

            endx = (*(iter_street + 1)).first;
            endy = (*(iter_street + 1)).second;

            setcolor(225, 225, 0, 225 * 0.25);
            setlinestyle(SOLID, ROUND);
            setlinewidth(20);
            drawline(startx, starty, endx, endy);
        }
    }

}

void react_to_restriction(float x, float y) {
    for (auto iter = (getAPI()->begin()); iter != getAPI()->end(); iter++) {
        float x_coord = ((iter->lontitude * cos(getIntersectionPosition(0).lat() * DEG_TO_RAD)));

        if (fabs(x - x_coord) < 0.0001 && fabs(iter->latitude - y) < 0.0001) {
            set_coordinate_system(GL_SCREEN);
            setcolor(135, 206, 250, 255);
            fillrect(500, 825, 1100, 870);
            setfontsize(8);
            setcolor(BLACK);
            drawtext(800, 850, iter->description);
            set_coordinate_system(GL_WORLD);
            //            information_bar.second = iter->description;
        }
    }

}
