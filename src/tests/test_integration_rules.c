/*
 * Unit Test for CPEX-0047 (Quadrature/Integration Rules)
 *
 * Validates:
 * - Rule Collections & Indexed Rules
 * - Parametric & Barycentric Rule Writes
 * - Element Associations
 * - Solution Integration Rules & Offsets
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "cgnslib.h"

#define IntegrationRule_s   "IntegrationRule"
#define Zone_ts            "Zone_t"
#define Elements_ts        "Elements_t"

typedef char char_33[33];

/* from CGNS internals to reset expected error message */
void cgi_error(const char* format, ...);

#define CHECK_FAILED(func) \
    if ((func) == CG_OK) { \
        printf("FAILED to produce error at line %d\n", __LINE__); \
        exit(1); \
    } else {  \
        cg_error_print(); \
        cgi_error("No CGNS Error reported"); \
    }

#define CHECK(func) \
    if ((func) != CG_OK) { \
        cg_error_print(); \
        printf("FAILED at line %d\n", __LINE__); \
        exit(1); \
    }

#define ASSERT(cond, msg) \
    if (!(cond)) { \
        printf("ASSERT FAILED: %s (Line %d)\n", msg, __LINE__); \
        exit(1); \
    }


void test_zone_element_association_node(){
    static char *fname = "elem_assoc.cgns";
    int cgfile, cgbase, cgzone, Sec_a, Sec_b, Sec_c, P_assoc;

    unlink(fname);
    // =================================================================
    // SETUP: Base and Zone
    // =================================================================
    if (cg_open(fname, CG_MODE_WRITE, &cgfile)) cg_error_exit();
    if (cg_base_write(cgfile, "Base", 3, 3, &cgbase)) cg_error_exit();
    
    cgsize_t sizes[3] = {10, 7, 0}; // 10 verts, 7 cells
    if (cg_zone_write(cgfile, cgbase, "Zone", sizes, Unstructured, &cgzone)) cg_error_exit();

    if (cg_section_partial_write(cgfile, cgbase, cgzone, "QUADS", CGNS_ENUMV(QUAD_4), 1, 3, 0, &Sec_a)) cg_error_exit();

    if (cg_section_partial_write(cgfile, cgbase, cgzone, "TRIS", CGNS_ENUMV(TRI_3), 4, 5, 0, &Sec_b)) cg_error_exit();

    if (cg_section_partial_write(cgfile, cgbase, cgzone, "MIXED", CGNS_ENUMV(MIXED), 6, 7, 0, &Sec_c)) cg_error_exit();

    // ===================================================================
    // WRITE: Element Association (Linking elements of a section to Rules)
    // ===================================================================
    printf("[WRITE] Element Associations...\n");
    cgsize_t assoc_num_ids = 1; // all quad cells use same rule, so store rule once.
    int assoc_data[1] = {10}; // Cells use Rule 10
    const char *path_to_rules = "/Base/QuadratureRules";

    // Create an Association under the Section.
    CHECK(cg_element_association_write(cgfile, cgbase, cgzone, Sec_a, 
                                       IntegrationRule_s, 
                                       path_to_rules, 
                                       assoc_num_ids, 
                                       assoc_data, 
                                       &P_assoc));

    // In WRITE mode it should fail as the element association with the same name already exist
    CHECK_FAILED(cg_element_association_write(cgfile, cgbase, cgzone, Sec_a, 
                                       IntegrationRule_s, 
                                       path_to_rules, 
                                       assoc_num_ids, 
                                       assoc_data, 
                                       &P_assoc));
    
    // Use a custom association name
    CHECK(cg_element_association_write(cgfile, cgbase, cgzone, Sec_a, 
                                       "MyInterpolationRule", 
                                       path_to_rules, 
                                       assoc_num_ids, 
                                       assoc_data, 
                                       &P_assoc));
    
    // Create Element association after a goto 
    int assoc_ids[1] = {20}; //All TRI Cells use Rule 20
    cg_goto(cgfile, cgbase, Zone_ts, cgzone, Elements_ts, Sec_b, NULL);
    CHECK(cg_association_write(IntegrationRule_s, path_to_rules, assoc_num_ids, &assoc_ids));

    int Pm_assoc;
    cgsize_t assoc_mixed_num_ids = 2; // 2 cells.
    int assoc_mixed_data[2] = {10, 20}; // Cell 1 uses Rule 10, Cell 2 uses Rule 20

     // Create an Association under the mixed Section.
    CHECK(cg_element_association_write(cgfile, cgbase, cgzone, Sec_c, 
                                       IntegrationRule_s, 
                                       path_to_rules, 
                                       assoc_mixed_num_ids, 
                                       assoc_mixed_data, 
                                       &Pm_assoc));

    if (cg_close(cgfile)) error_exit("cg_close");

    // ===================================================================
    // READ or MODIFY: Element Association
    // ===================================================================
    printf("[READ] Element Associations...\n");
    if (cg_open(fname, CG_MODE_MODIFY, &cgfile)) error_exit("cg_open");
    
    printf("[READ] Deleting and rewriting Association...\n");
    cg_goto(cgfile, cgbase, Zone_ts, cgzone, Elements_ts, Sec_b, NULL);
    cg_delete_node(IntegrationRule_s);
    assoc_ids[0] = 15;
    CHECK(cg_association_write(IntegrationRule_s, path_to_rules, assoc_num_ids, assoc_ids));

    printf("[READ] Checking written Associations...\n");
    int nprops;
    CHECK(cg_nelement_associations(cgfile, cgbase, cgzone, Sec_a, &nprops));
    ASSERT(nprops==2, "Invalid number of element association in QUAD section");
    CHECK(cg_nelement_associations(cgfile, cgbase, cgzone, Sec_b, &nprops));
    ASSERT(nprops==1, "Invalid number of element association in TRI section");
    CHECK(cg_nelement_associations(cgfile, cgbase, cgzone, Sec_c, &nprops));
    ASSERT(nprops==1, "Invalid number of element association in MIXED section");
    
    cgsize_t num_ids;
    char *path = NULL;
    char nodename[33];
    CHECK(cg_element_association_info(cgfile, cgbase, cgzone, Sec_c, 1, nodename, &path, &num_ids));
    ASSERT(strcmp(nodename, IntegrationRule_s) == 0, "Incorrect element association node name")
    ASSERT(strcmp(path, path_to_rules) == 0, "Incorrect path to rules detected");
    ASSERT(num_ids == assoc_mixed_num_ids, "Number of mapped ids is not valid");
    free(path); path = NULL;

    int data[2];
    CHECK(cg_element_association_read(cgfile, cgbase, cgzone, Sec_c, 1, data));
    ASSERT(data[0]==assoc_mixed_data[0], "First element association is wrong");
    ASSERT(data[1]==assoc_mixed_data[1], "Second element association is wrong");

    /* Now check that modification was done and global mapping works */
    CHECK(cg_element_association_info(cgfile, cgbase, cgzone, Sec_b, 1, nodename, &path, &num_ids));
    ASSERT(strcmp(nodename, IntegrationRule_s) == 0, "Incorrect element association node name");
    ASSERT(strcmp(path, path_to_rules) == 0, "Incorrect path to rules detected");
    ASSERT(num_ids == assoc_num_ids, "Number of mapped ids should be 1");
    free(path); path = NULL;

    CHECK(cg_element_association_read(cgfile, cgbase, cgzone, Sec_b, 1, data));
    ASSERT(data[0]==assoc_ids[0], "Global association is wrong")

    /* Get info with goto */
    cg_goto(cgfile, cgbase, Zone_ts, cgzone, Elements_ts, Sec_a, NULL);
    CHECK(cg_nassociations(&nprops));
    ASSERT(nprops==2, "Invalid number of element association in QUAD section");
    CHECK(cg_association_info(2, nodename, &path, &num_ids));
    ASSERT(strcmp(nodename, "MyInterpolationRule") == 0, "Incorrect element association node name");
    ASSERT(strcmp(path, path_to_rules) == 0, "Incorrect path to rules detected");
    ASSERT(num_ids == assoc_num_ids, "Number of mapped ids is not valid");

    if (cg_close(cgfile)) error_exit("cg_close");

}

void test_solution_element_association_node(){
    static char *fname = "elem_sol_assoc.cgns";
    int cgfile, cgbase, cgzone, cgsol, Sec_a, Sec_b, Sec_c, P_assoc;

    unlink(fname);
    /* SETUP: Base, Zone and Solution at Integration Point */
    if (cg_open(fname, CG_MODE_WRITE, &cgfile)) cg_error_exit();
    if (cg_base_write(cgfile, "Base", 3, 3, &cgbase)) cg_error_exit();
    
    cgsize_t sizes[3] = {10, 7, 0}; // 10 verts, 7 cells
    if (cg_zone_write(cgfile, cgbase, "Zone", sizes, Unstructured, &cgzone)) cg_error_exit();

    if (cg_section_partial_write(cgfile, cgbase, cgzone, "QUADS", CGNS_ENUMV(QUAD_4), 1, 3, 0, &Sec_a)) cg_error_exit();

    if (cg_section_partial_write(cgfile, cgbase, cgzone, "TRIS", CGNS_ENUMV(TRI_3), 4, 5, 0, &Sec_b)) cg_error_exit();

    if (cg_section_partial_write(cgfile, cgbase, cgzone, "MIXED", CGNS_ENUMV(MIXED), 6, 7, 0, &Sec_c)) cg_error_exit();

    printf("[WRITE] Integration Rule Element Associations at Solution level...\n");
    /* New Solution at integration Point */
    CHECK(cg_sol_write(cgfile, cgbase, cgzone, "solutionIP", CGNS_ENUMV(IntegrationPoint), &cgsol));

    /* Create association at solution level */
    cgsize_t assoc_num_ids = 7; // each cell gets its value
    int assoc_data[7] = {1, 1, 1, 3, 3, 2, 2};
    const char *path_to_rules = "/Base/QuadratureRules";
    CHECK(cg_sol_itg_rule_write(cgfile, cgbase, cgzone, cgsol, path_to_rules, assoc_num_ids, &assoc_data));

    if (cg_close(cgfile)) error_exit("cg_close");

    printf("[READ] Integration Rule Element Association at solution level...\n");
    if (cg_open(fname, CG_MODE_READ, &cgfile)) error_exit("cg_open");

    cgsize_t num_ids;
    char *path = NULL;
    int data[7];
    CHECK(cg_sol_itg_rule_info(cgfile, cgbase, cgzone, cgsol, &path, &num_ids));
    ASSERT(strcmp(path, path_to_rules) == 0, "Incorrect path to rules detected in solution");
    ASSERT(num_ids == assoc_num_ids, "Number of mapped ids is wrong");
    free(path); path = NULL;

    CHECK(cg_sol_itg_rule_read(cgfile, cgbase, cgzone, cgsol, data));
    ASSERT(data[0] == assoc_data[0], "First associated integration rule is wrong");
    ASSERT(data[3] == assoc_data[3], "Associated integration rule is wrong");
    ASSERT(data[6] == assoc_data[6], "Associated integration rule is wrong");
            
    cg_goto(cgfile, cgbase, "Zone_t", cgzone, "FlowSolution_t", cgsol, "end");
    cg_delete_node(IntegrationRule_s);
    /* No association should be found */
    CHECK_FAIL(cg_sol_itg_rule_info(cgfile, cgbase, cgzone, cgsol, &path, &num_ids));

    if (cg_close(cgfile)) error_exit("cg_close");
}

void test_integration_rule_definition(){
    static char *fname = "elem_itg_rule_def.cgns";
    int cgfile, cgbase, cgzone, cgrule1, cgrule2, cgrule3, Sec_a, Sec_b, Sec_c, P_assoc;
    int itgtetra, itgpyra, itghex, itgpenta, itgtri, itgquad, itgbaryquad, itgbarytri;

    unlink(fname);
    /* SETUP: Base, Zone */
    if (cg_open(fname, CG_MODE_WRITE, &cgfile)) cg_error_exit();
    if (cg_base_write(cgfile, "Base", 3, 3, &cgbase)) cg_error_exit();
    
    cgsize_t sizes[3] = {10, 7, 0}; // 10 verts, 7 cells
    if (cg_zone_write(cgfile, cgbase, "Zone", sizes, Unstructured, &cgzone)) cg_error_exit();


    printf("[WRITE] Integration Rule Collection...\n");
    /* New rule collections to store integration rules */
    CHECK(cg_rcollection_write(cgfile, cgbase, "QuadratureRules", &cgrule1));
    CHECK(cg_rcollection_write(cgfile, cgbase, "IRCollection2", &cgrule2));
    CHECK(cg_rcollection_write(cgfile, cgbase, "IRCollection3", &cgrule3));

    printf("[WRITE] Integration Rule for element type...\n");
    /* Integration rule for TETRA */
        double coeff_a = (5.0 - sqrt(5.0)) / 20.;
        double coeff_b = sqrt(5.0) / 5.0;
        double param[3][4];
        param[0][0] = 0.0;
        param[1][0] = 0.0;
        param[2][0] = 0.0;
        param[0][1] = 0.0;
        param[1][1] = 0.0;
        param[2][1] = 1.0;
        param[0][2] = 0.0;
        param[1][2] = 1.0;
        param[2][2] = 0.0;
        param[0][3] = 1.0;
        param[1][3] = 0.0;
        param[2][3] = 0.0;
        for (int ii = 0; ii < 3; ii++) {
            for (int jj = 0; jj < 4; jj++) {
                param[ii][jj] *= coeff_b;
                param[ii][jj] += coeff_a;
            }
        }
        double weights[4] = {1.0 / 24., 1.0 / 24., 1.0 / 24., 1.0 / 24.};
        CHECK(cg_itg_rule_parametric_write(cgfile, cgbase, cgrule1, "tetra", TETRA_4, 4, 3,
             RealDouble, param, weights, &itgtetra));

        /* Integration rule for pyra */
        double param_pyra[3][1] = {{0.0}, {0.0}, {1. / 5.}};
        double weights_pyra[1] = {2. / 3.};

        CHECK(cg_itg_rule_parametric_write(cgfile, cgbase, cgrule2, "pyra", PYRA_5, 1, 3,
             RealDouble, param_pyra, weights_pyra, &itgpyra));

        /* Integration rule for Hexa */
        double coeff = sqrt(3.) / 3.;
        double param_hex[3][8];
        param_hex[0][0] = -1.0;
        param_hex[1][0] = -1.0;
        param_hex[2][0] = -1.0;
        param_hex[0][1] = -1.0;
        param_hex[1][1] = -1.0;
        param_hex[2][1] = 1.0;
        param_hex[0][2] = -1.0;
        param_hex[1][2] = 1.0;
        param_hex[2][2] = -1.0;
        param_hex[0][3] = -1.0;
        param_hex[1][3] = 1.0;
        param_hex[2][3] = 1.0;
        param_hex[0][4] = 1.0;
        param_hex[1][4] = -1.0;
        param_hex[2][4] = -1.0;
        param_hex[0][5] = 1.0;
        param_hex[1][5] = -1.0;
        param_hex[2][5] = 1.0;
        param_hex[0][6] = 1.0;
        param_hex[1][6] = 1.0;
        param_hex[2][6] = -1.0;
        param_hex[0][7] = 1.0;
        param_hex[1][7] = 1.0;
        param_hex[2][7] = 1.0;
        for (int ii = 0; ii < 3; ii++) {
            for (int jj = 0; jj < 8; jj++) {
                param_hex[ii][jj] *= coeff;
                param_hex[ii][jj] =
                    (param_hex[ii][jj] + 1.0) * 0.5; // Transform to [0.0, 1.0] interval
            }
        }
        double weights_hex[8] = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0};

        CHECK(cg_itg_rule_parametric_write(cgfile, cgbase, cgrule2, "hexa", HEXA_8, 8, 3,
                                     RealDouble, param_hex, weights_hex, &itghex));

        /* Integration rule for Penta */
        double param_penta[3][2];
        param_penta[0][0] = -1.0 / sqrt(3.0);
        param_penta[1][0] = 1.0 / 3.0;
        param_penta[2][0] = 1.0 / 3.0;
        param_penta[0][1] = 1.0 / sqrt(3.0);
        param_penta[1][1] = 1.0 / 3.0;
        param_penta[2][1] = 1.0 / 3.0;

        double weights_penta[2] = {0.5, 0.5};

        CHECK(cg_itg_rule_parametric_write(cgfile, cgbase, cgrule1, "penta", PENTA_6, 2, 3,
                                     RealDouble, param_penta, weights_penta, &itgpenta));

        /* Integration rule for Tri */
        double param_tri[2][1];
        param_tri[0][0] = 1.0 / 3.0;
        param_tri[1][0] = 1.0 / 3.0;

        double weights_tri[1] = {0.5};
        CHECK(cg_itg_rule_parametric_write(cgfile, cgbase, cgrule2, "tri", TRI_3, 1, 2,
                                     RealDouble, param_tri, weights_tri, &itgtri));

        /* Integration rule for Quad */
        double param_quad[2][4];
        param_quad[0][0] = -1.0 / sqrt(3.0);
        param_quad[1][0] = -1.0 / sqrt(3.0);
        param_quad[0][1] = -1.0 / sqrt(3.0);
        param_quad[1][1] = 1.0 / sqrt(3.0);
        param_quad[0][2] = 1.0 / sqrt(3.0);
        param_quad[1][2] = -1.0 / sqrt(3.0);
        param_quad[0][3] = 1.0 / sqrt(3.0);
        param_quad[1][3] = 1.0 / sqrt(3.0);

        double weights_quad[4] = {1.0, 1.0, 1.0, 1.0};
        //
        for (int ii = 0; ii < 2; ii++) {
            for (int jj = 0; jj < 4; jj++) {
                param_quad[ii][jj] =
                    (param_quad[ii][jj] + 1.0) * 0.5; // Transform to [0.0, 1.0] interval
            }
        }
        CHECK(cg_itg_rule_parametric_write(cgfile, cgbase, cgrule2, "quad", QUAD_4, 4, 2,
                                     RealDouble, param_penta, weights_quad, &itgquad));

       /* Integration rule for Tri_3 */
        double pos_itg_tri[3][1];

        pos_itg_tri[0][0] = 1.0 / 3.0;
        pos_itg_tri[1][0] = 1.0 / 3.0;
        pos_itg_tri[2][0] = 1.0 / 3.0;

        double weights_barytri[1] = {0.5};
        CHECK(cg_itg_rule_barycentric_write(cgfile, cgbase, cgrule3, "barytri", TRI_3, 1, 3,
                                      RealDouble, pos_itg_tri, weights_barytri,
                                      &itgbarytri));
        /* Barycentric rules for Quad */
        double pos_itg_quad[4][4];
        pos_itg_quad[0][0] = (1 + sqrt(3)) / 4;
        pos_itg_quad[0][1] = (1 - 1 / sqrt(3)) / 4;
        pos_itg_quad[0][2] = (1 - 1 / sqrt(3)) / 4;
        pos_itg_quad[0][3] = (1 - 1 / sqrt(3)) / 4;
        pos_itg_quad[1][0] = (1 - 1 / sqrt(3)) / 4;
        pos_itg_quad[1][1] = (1 + sqrt(3)) / 4;
        pos_itg_quad[1][2] = (1 - 1 / sqrt(3)) / 4;
        pos_itg_quad[1][3] = (1 - 1 / sqrt(3)) / 4;
        pos_itg_quad[2][0] = (1 - 1 / sqrt(3)) / 4;
        pos_itg_quad[2][1] = (1 - 1 / sqrt(3)) / 4;
        pos_itg_quad[2][2] = (1 + sqrt(3)) / 4;
        pos_itg_quad[2][3] = (1 - 1 / sqrt(3)) / 4;
        pos_itg_quad[3][0] = (1 - 1 / sqrt(3)) / 4;
        pos_itg_quad[3][1] = (1 - 1 / sqrt(3)) / 4;
        pos_itg_quad[3][2] = (1 - 1 / sqrt(3)) / 4;
        pos_itg_quad[3][3] = (1 + sqrt(3)) / 4;
        double weights_baryquad[4] = {1.0, 1.0, 1.0, 1.0};

        CHECK(cg_itg_rule_barycentric_write(cgfile, cgbase, cgrule3, "baryquad", QUAD_4, 4, 4,
                                      RealDouble, pos_itg_quad, weights_baryquad,
                                      &itgbaryquad));

    /* Create association id to rule node name */
    int rule_ids_1[2] = {10, 20};
    char_33 *rule_names_1 = (char_33 *)malloc(2 * sizeof(char_33));
    strcpy(rule_names_1[0], "tetra");
    strcpy(rule_names_1[1], "penta");

    CHECK(cg_rcollection_idtoqualifier_write(cgfile, cgbase, cgrule1, 2, rule_ids_1, (char *)(rule_names_1)));
    free(rule_names_1);

    /* Create association for second set of rules */
    int rule_ids_2[4] = {187, 186, 123, 181};
    char_33 *rule_names_2 = (char_33 *)malloc(4 * sizeof(char_33));
    strcpy(rule_names_2[0], "pyra");
    strcpy(rule_names_2[1], "hexa");
    strcpy(rule_names_2[2], "tri");
    strcpy(rule_names_2[3], "quad");

    CHECK(cg_rcollection_idtoqualifier_write(cgfile, cgbase, cgrule2, 4, rule_ids_2, (char *)(rule_names_2)));
    free(rule_names_2);

    
    /* Create association for third set of rules */
    int rule_ids_3[4] = {4, 3};
    char_33 *rule_names_3 = (char_33 *)malloc(2 * sizeof(char_33));
    strcpy(rule_names_3[0], "bary_tri");
    strcpy(rule_names_3[1], "bary_quad");
    CHECK(cg_rcollection_idtoqualifier_write(cgfile, cgbase, cgrule3, 2, rule_ids_3, (char *)(rule_names_3)));
    free(rule_names_3);
    
    if (cg_close(cgfile)) error_exit("cg_close");

    printf("[READ] Checking Integration Rule ...\n");

    if (cg_open(fname, CG_MODE_READ, &cgfile)) error_exit("cg_open");

    int ncollection = 0;
    CHECK(cg_nrcollections(cgfile, cgbase, &ncollection));
    ASSERT(ncollection==3, "Number of rules collections invalid");
    char_33 colname;
    CHECK(cg_rcollection_read(cgfile, cgbase, 1, colname));
    ASSERT(strcmp(colname, "QuadratureRules") == 0, "Incorrect collection name")
    CHECK(cg_rcollection_read(cgfile, cgbase, 2, colname));
    ASSERT(strcmp(colname, "IRCollection2") == 0, "Incorrect collection name")
    CHECK(cg_rcollection_read(cgfile, cgbase, 3, colname));
    ASSERT(strcmp(colname, "IRCollection3") == 0, "Incorrect collection name")

    int num_rules = 0;
    CHECK(cg_rcollection_nindexed_rules(cgfile, cgbase, 1, &num_rules));
    ASSERT(num_rules==2, "Incorrect number of mapped rules")
    CHECK(cg_rcollection_nindexed_rules(cgfile, cgbase, 2, &num_rules));
    ASSERT(num_rules==4, "Incorrect number of mapped rules")
    CHECK(cg_rcollection_nindexed_rules(cgfile, cgbase, 3, &num_rules));
    ASSERT(num_rules==2, "Incorrect number of mapped rules")
    
    int rule_check_ids_1[2];
    rule_names_1 = (char_33 *)malloc(2 * sizeof(char_33));
    CHECK(cg_rcollection_idtoqualifier_read(cgfile, cgbase, 1, rule_check_ids_1, (char *)rule_names_1));
    ASSERT(rule_check_ids_1[0] == rule_ids_1[0], "Mapped Id is incorrect")
    ASSERT(rule_check_ids_1[1] == rule_ids_1[1], "Mapped Id is incorrect")
    ASSERT(strcmp(rule_names_1[0], "tetra") == 0, "Invalid mapped rule name")
    ASSERT(strcmp(rule_names_1[1], "penta") == 0, "Invalid mapped rule name")
    free(rule_names_1);
    
    

    if (cg_close(cgfile)) error_exit("cg_close");
}