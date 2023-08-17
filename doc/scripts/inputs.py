import representation
from representation import Segment
############# iexpr (label_morph)

iexpr_directional_loc = {"type": "locset", "value": [(0, 1.0)]}
iexpr_dist_dis = {
    "type": "region",
    "value": [(1, 0.0, 1.0), (2, 0.0, 1.0), (3, 0.0, 1.0), (4, 0.0, 1.0)],
}
iexpr_prox_dis = {"type": "region", "value": [(0, 0.0, 1.0)]}

############# morphologies

tmp = [
    [[Segment((0.0, 0.0, 2.0), (4.0, 0.0, 2.0), 1), Segment((4.0, 0.0, 0.8), (8.0, 0.0, 0.8), 3), Segment((8.0, 0.0, 0.8), (12.0, -0.5, 0.8), 3)]],
    [[Segment((12.0, -0.5, 0.8), (20.0, 4.0, 0.4), 3), Segment((20.0, 4.0, 0.4), (26.0, 6.0, 0.2), 3)]],
    [[Segment((12.0, -0.5, 0.5), (19.0, -3.0, 0.5), 3)]],
    [[Segment((19.0, -3.0, 0.5), (24.0, -7.0, 0.2), 3)]],
    [[Segment((19.0, -3.0, 0.5), (23.0, -1.0, 0.2), 3), Segment((23.0, -1.0, 0.2), (26.0, -2.0, 0.2), 3)]],
    [[Segment((0.0, 0.0, 2.0), (-7.0, 0.0, 0.4), 2), Segment((-7.0, 0.0, 0.4), (-10.0, 0.0, 0.4), 2)]],]
label_morph = representation.make_morph(tmp)

tmp = [
    [[Segment((0.0, 0.0, 2.0), (4.0, 0.0, 2.0), 1)], [Segment((5.0, 0.0, 0.8), (8.0, 0.0, 0.8), 3), Segment((8.0, 0.0, 0.8), (12.0, -0.5, 0.8), 3)]],
    [[Segment((12.0, -0.5, 0.8), (20.0, 4.0, 0.4), 3), Segment((20.0, 4.0, 0.4), (26.0, 6.0, 0.2), 3)]],
    [[Segment((12.0, -0.5, 0.5), (19.0, -3.0, 0.5), 3)]],
    [[Segment((19.0, -3.0, 0.5), (24.0, -7.0, 0.2), 3)]],
    [[Segment((19.0, -3.0, 0.5), (23.0, -1.0, 0.2), 3), Segment((23.0, -1.0, 0.2), (26.0, -2.0, 0.2), 3)]],
    [[Segment((-2.0, 0.0, 0.4), (-10.0, 0.0, 0.4), 2)]],]
detached_morph = representation.make_morph(tmp)

tmp = [
    [[Segment((0.0, 0.0, 0.5), (1.0, 0.0, 1.5), 1), Segment((1.0, 0.0, 1.5), (2.0, 0.0, 2.5), 1), Segment((2.0, 0.0, 2.5), (3.0, 0.0, 2.5), 1), Segment((3.0, 0.0, 2.5), (4.0, 0.0, 1.2), 1), Segment((4.0, 0.0, 0.8), (8.0, 0.0, 0.8), 3), Segment((8.0, 0.0, 0.8), (12.0, -0.5, 0.8), 3)]],
    [[Segment((12.0, -0.5, 0.8), (20.0, 4.0, 0.4), 3), Segment((20.0, 4.0, 0.4), (26.0, 6.0, 0.2), 3)]],
    [[Segment((12.0, -0.5, 0.5), (19.0, -3.0, 0.5), 3)]],
    [[Segment((19.0, -3.0, 0.5), (24.0, -7.0, 0.2), 3)]],
    [[Segment((19.0, -3.0, 0.5), (23.0, -1.0, 0.2), 3), Segment((23.0, -1.0, 0.2), (26.0, -2.0, 0.2), 3)]],
    [[Segment((0.0, 0.0, 0.4), (-7.0, 0.0, 0.4), 2), Segment((-7.0, 0.0, 0.4), (-10.0, 0.0, 0.4), 2)]],]
stacked_morph = representation.make_morph(tmp)

tmp = [
    [[Segment((-2.0, 0.0, 2.0), (2.0, 0.0, 2.0), 1)]],]
sphere_morph = representation.make_morph(tmp)

tmp = [
    [[Segment((0.0, 0.0, 1.0), (10.0, 0.0, 0.5), 3)]],]
branch_morph1 = representation.make_morph(tmp)

tmp = [
    [[Segment((0.0, 0.0, 1.0), (3.0, 0.2, 0.8), 1), Segment((3.0, 0.2, 0.8), (5.0, -0.1, 0.7), 2), Segment((5.0, -0.1, 0.7), (8.0, 0.0, 0.6), 2), Segment((8.0, 0.0, 0.6), (10.0, 0.0, 0.5), 3)]],]
branch_morph2 = representation.make_morph(tmp)

tmp = [
    [[Segment((0.0, 0.0, 1.0), (3.0, 0.2, 0.8), 1), Segment((3.0, 0.2, 0.8), (5.0, -0.1, 0.7), 2)], [Segment((6.0, -0.1, 0.7), (9.0, 0.0, 0.6), 2), Segment((9.0, 0.0, 0.6), (11.0, 0.0, 0.5), 3)]],]
branch_morph3 = representation.make_morph(tmp)

tmp = [
    [[Segment((0.0, 0.0, 1.0), (3.0, 0.2, 0.8), 1), Segment((3.0, 0.2, 0.8), (5.0, -0.1, 0.7), 2), Segment((5.0, -0.1, 0.7), (8.0, 0.0, 0.5), 2), Segment((8.0, 0.0, 0.3), (10.0, 0.0, 0.5), 3)]],]
branch_morph4 = representation.make_morph(tmp)

tmp = [
    [[Segment((0.0, 0.0, 1.0), (10.0, 0.0, 0.5), 3)]],
    [[Segment((10.0, 0.0, 0.5), (15.0, 3.0, 0.2), 3)]],
    [[Segment((10.0, 0.0, 0.5), (15.0, -3.0, 0.2), 3)]],]
yshaped_morph = representation.make_morph(tmp)

tmp = [
    [[Segment((-3.0, 0.0, 3.0), (3.0, 0.0, 3.0), 1)], [Segment((4.0, -1.0, 0.6), (10.0, -2.0, 0.5), 3), Segment((10.0, -2.0, 0.5), (15.0, -1.0, 0.5), 3)]],
    [[Segment((15.0, -1.0, 0.5), (18.0, -5.0, 0.3), 3)]],
    [[Segment((15.0, -1.0, 0.5), (20.0, 2.0, 0.3), 3)]],]
ysoma_morph1 = representation.make_morph(tmp)

tmp = [
    [[Segment((-3.0, 0.0, 3.0), (3.0, 0.0, 3.0), 1)]],
    [[Segment((4.0, -1.0, 0.6), (10.0, -2.0, 0.5), 3), Segment((10.0, -2.0, 0.5), (15.0, -1.0, 0.5), 3)]],
    [[Segment((15.0, -1.0, 0.5), (18.0, -5.0, 0.3), 3)]],
    [[Segment((15.0, -1.0, 0.5), (20.0, 2.0, 0.3), 3)]],
    [[Segment((2.0, 1.0, 0.6), (12.0, 4.0, 0.5), 3)]],
    [[Segment((12.0, 4.0, 0.5), (18.0, 4.0, 0.3), 3)]],
    [[Segment((12.0, 4.0, 0.5), (16.0, 9.0, 0.1), 3)]],
    [[Segment((-3.5, 0.0, 1.5), (-6.0, -0.2, 0.5), 2), Segment((-6.0, -0.2, 0.5), (-15.0, -0.1, 0.5), 2)]],]
ysoma_morph2 = representation.make_morph(tmp)

tmp = [
    [[Segment((-3.0, 0.0, 3.0), (3.0, 0.0, 3.0), 1)]],
    [[Segment((3.0, 0.0, 0.6), (9.0, -1.0, 0.5), 3), Segment((9.0, -1.0, 0.5), (14.0, 0.0, 0.5), 3)]],
    [[Segment((14.0, 0.0, 0.5), (17.0, -4.0, 0.3), 3)]],
    [[Segment((14.0, 0.0, 0.5), (19.0, 3.0, 0.3), 3)]],
    [[Segment((3.0, 0.0, 0.6), (13.0, 3.0, 0.5), 3)]],
    [[Segment((13.0, 3.0, 0.5), (19.0, 3.0, 0.3), 3)]],
    [[Segment((13.0, 3.0, 0.5), (17.0, 8.0, 0.1), 3)]],
    [[Segment((-3.0, 0.0, 1.5), (-5.5, -0.2, 0.5), 2), Segment((-5.5, -0.2, 0.5), (-14.5, -0.1, 0.5), 2)]],]
ysoma_morph3 = representation.make_morph(tmp)

tmp = [
    [[Segment((0.0, 0.0, 2.0), (4.0, 0.0, 2.0), 1), Segment((4.0, 0.0, 0.8), (8.0, 0.0, 0.8), 3), Segment((8.0, 0.0, 0.8), (12.0, -0.5, 0.8), 3)]],
    [[Segment((12.0, -0.5, 0.8), (20.0, 4.0, 0.4), 3), Segment((20.0, 4.0, 0.4), (26.0, 6.0, 0.2), 3)]],
    [[Segment((12.0, -0.5, 0.5), (19.0, -3.0, 0.5), 3)]],
    [[Segment((19.0, -3.0, 0.5), (24.0, -7.0, 0.2), 4)]],
    [[Segment((19.0, -3.0, 0.5), (23.0, -1.0, 0.2), 4), Segment((23.0, -1.0, 0.2), (36.0, -2.0, 0.2), 4)]],
    [[Segment((0.0, 0.0, 2.0), (-7.0, 0.0, 0.4), 2), Segment((-7.0, 0.0, 0.4), (-10.0, 0.0, 0.4), 2)]],]
tutorial_morph = representation.make_morph(tmp)

tmp = [
    [[Segment((0.0, 0.0, 1.0), (2.0, 0.0, 1.0), 1), Segment((2.0, 0.0, 1.0), (20.0, 0.0, 1.0), 3)]],
    [[Segment((0.0, 0.0, 1.0), (-3.0, 0.0, 0.7), 2)]],]
swc_morph = representation.make_morph(tmp)

tmp = [
    [[Segment((-12.0, 0.0, 6.0), (0.0, 0.0, 6.0), 1), Segment((0.0, 0.0, 2.0), (50.0, 0.0, 2.0), 3)]],
    [[Segment((50.0, 0.0, 2.0), (85.35533905932738, 35.35533905932737, 0.5), 3)]],
    [[Segment((50.0, 0.0, 1.0), (85.35533905932738, -35.35533905932737, 1.0), 3)]],]
tutorial_network_ring_morph = representation.make_morph(tmp)


############# locsets (label_morph)

ls_root  = {'type': 'locset', 'value': [(0, 0.0)]}
ls_term  = {'type': 'locset', 'value': [(1, 1.0), (3, 1.0), (4, 1.0), (5, 1.0)]}
ls_rand_dend  = {'type': 'locset', 'value': [(0, 0.5547193370156588), (0, 0.5841758202819731), (0, 0.607192003545501), (0, 0.6181091003428546), (0, 0.6190845627201184), (0, 0.7027325639263277), (0, 0.7616129092226993), (0, 0.9645150497869694), (1, 0.15382287505908834), (1, 0.2594719824047551), (1, 0.28087652335178354), (1, 0.3729681478609085), (1, 0.3959560134241004), (1, 0.4629424550242548), (1, 0.47346867377446744), (1, 0.5493486883630476), (1, 0.6227685370674116), (1, 0.6362196581003494), (1, 0.6646511214508091), (1, 0.7157318936458146), (1, 0.7464198558822775), (1, 0.77074507802833), (1, 0.7860238136304932), (1, 0.8988928261704698), (1, 0.9581259332943499), (2, 0.12773985425987294), (2, 0.3365926476076694), (2, 0.44454300804769703), (2, 0.5409466695719178), (2, 0.5767511435223905), (2, 0.6340206909931745), (2, 0.6354772583375223), (2, 0.6807941995943213), (2, 0.774655947503608), (3, 0.05020708596877571), (3, 0.25581431877212274), (3, 0.2958305460715556), (3, 0.296698184761692), (3, 0.509669134988683), (3, 0.7662305637426007), (3, 0.8565839889923518), (3, 0.8889077221517746), (4, 0.24311286693286885), (4, 0.4354361205546333), (4, 0.4467752481260171), (4, 0.5308169153994543), (4, 0.5701465671464049), (4, 0.670081739879954), (4, 0.6995486862583797), (4, 0.8186709628604206), (4, 0.9141224600171143)]}
ls_loc15  = {'type': 'locset', 'value': [(1, 0.5)]}
ls_loc05  = {'type': 'locset', 'value': [(0, 0.5)]}
ls_uniform0  = {'type': 'locset', 'value': [(0, 0.5841758202819731), (1, 0.6362196581003494), (1, 0.7157318936458146), (1, 0.7464198558822775), (2, 0.6340206909931745), (2, 0.6807941995943213), (3, 0.296698184761692), (3, 0.509669134988683), (3, 0.7662305637426007), (4, 0.5701465671464049)]}
ls_uniform1  = {'type': 'locset', 'value': [(0, 0.9778060763285382), (1, 0.19973428495790843), (1, 0.8310607916260988), (2, 0.9210229159315735), (2, 0.9244292525837472), (2, 0.9899772550845479), (3, 0.9924233395972087), (4, 0.3641426305909531), (4, 0.4787812247064867), (4, 0.5138656268861914)]}
ls_branchmid  = {'type': 'locset', 'value': [(0, 0.5), (1, 0.5), (2, 0.5), (3, 0.5), (4, 0.5), (5, 0.5)]}
ls_componentsmid  = {'type': 'locset', 'value': [(1, 0.24098705946874843), (2, 0.5026349903620757)]}
ls_boundary  = {'type': 'locset', 'value': [(0, 0.6649417593048336), (0, 1.0)]}
ls_cboundary  = {'type': 'locset', 'value': [(0, 0.6649417593048336), (1, 0.0), (2, 0.0)]}
ls_sboundary  = {'type': 'locset', 'value': [(0, 0.0), (0, 0.3324708796524168), (0, 0.6649417593048336), (0, 1.0), (1, 0.0), (1, 0.5920519526598877), (1, 1.0), (2, 0.0), (2, 1.0), (3, 0.0), (3, 1.0), (4, 0.0), (4, 0.585786437626905), (4, 1.0), (5, 0.0), (5, 0.7), (5, 1.0)]}
ls_distal  = {'type': 'locset', 'value': [(1, 0.796025976329944), (3, 0.6666666666666667), (4, 0.39052429175127), (5, 1.0)]}
ls_proximal  = {'type': 'locset', 'value': [(1, 0.29602597632994393), (2, 0.0), (5, 0.6124999999999999)]}
ls_distint_in  = {'type': 'locset', 'value': [(1, 0.5), (2, 0.7), (5, 0.1)]}
ls_proxint_in  = {'type': 'locset', 'value': [(1, 0.8), (2, 0.3)]}
ls_loctest  = {'type': 'locset', 'value': [(1, 1.0), (2, 0.0), (5, 0.0)]}
ls_restrict  = {'type': 'locset', 'value': [(1, 1.0), (3, 1.0), (4, 1.0)]}
ls_proximal_translate  = {'type': 'locset', 'value': [(1, 0.35497750169352515), (2, 0.5160959062272675), (2, 0.6817468794150789), (5, 0.0)]}
ls_distal_translate_single  = {'type': 'locset', 'value': [(0, 0.915588599565521)]}
ls_distal_translate_multi  = {'type': 'locset', 'value': [(1, 0.5795163072671657), (3, 0.24228815992614555), (4, 0.20321157163712014)]}

############# regions (label_morph)

reg_empty = {'type': 'region', 'value': []}
reg_all = {'type': 'region', 'value': [(0, 0.0, 1.0), (1, 0.0, 1.0), (2, 0.0, 1.0), (3, 0.0, 1.0), (4, 0.0, 1.0), (5, 0.0, 1.0)]}
reg_tag1 = {'type': 'region', 'value': [(0, 0.0, 0.3324708796524168)]}
reg_tag2 = {'type': 'region', 'value': [(5, 0.0, 1.0)]}
reg_tag3 = {'type': 'region', 'value': [(0, 0.3324708796524168, 1.0), (1, 0.0, 1.0), (2, 0.0, 1.0), (3, 0.0, 1.0), (4, 0.0, 1.0)]}
reg_tag4 = {'type': 'region', 'value': []}
reg_soma = {'type': 'region', 'value': [(0, 0.0, 0.3324708796524168)]}
reg_axon = {'type': 'region', 'value': [(5, 0.0, 1.0)]}
reg_dend = {'type': 'region', 'value': [(0, 0.3324708796524168, 1.0), (1, 0.0, 1.0), (2, 0.0, 1.0), (3, 0.0, 1.0), (4, 0.0, 1.0)]}
reg_radlt5 = {'type': 'region', 'value': [(1, 0.44403896449491587, 1.0), (3, 0.0, 1.0), (4, 0.0, 1.0), (5, 0.65625, 1.0)]}
reg_radle5 = {'type': 'region', 'value': [(1, 0.44403896449491587, 1.0), (2, 0.0, 1.0), (3, 0.0, 1.0), (4, 0.0, 1.0), (5, 0.65625, 1.0)]}
reg_radgt5 = {'type': 'region', 'value': [(0, 0.0, 1.0), (1, 0.0, 0.44403896449491587), (5, 0.0, 0.65625)]}
reg_radge5 = {'type': 'region', 'value': [(0, 0.0, 1.0), (1, 0.0, 0.44403896449491587), (2, 0.0, 1.0), (3, 0.0, 0.0), (4, 0.0, 0.0), (5, 0.0, 0.65625)]}
reg_rad36 = {'type': 'region', 'value': [(1, 0.29602597632994393, 0.796025976329944), (2, 0.0, 1.0), (3, 0.0, 0.6666666666666667), (4, 0.0, 0.39052429175127), (5, 0.6124999999999999, 1.0)]}
reg_branch0 = {'type': 'region', 'value': [(0, 0.0, 1.0)]}
reg_branch3 = {'type': 'region', 'value': [(3, 0.0, 1.0)]}
reg_segment0 = {'type': 'region', 'value': [(0, 0.0, 0.3324708796524168)]}
reg_segment3 = {'type': 'region', 'value': [(1, 0.0, 0.5920519526598877)]}
reg_cable_0_28 = {'type': 'region', 'value': [(0, 0.2, 0.8)]}
reg_cable_1_01 = {'type': 'region', 'value': [(1, 0.0, 1.0)]}
reg_cable_1_31 = {'type': 'region', 'value': [(1, 0.3, 1.0)]}
reg_cable_1_37 = {'type': 'region', 'value': [(1, 0.3, 0.7)]}
reg_proxint = {'type': 'region', 'value': [(0, 0.7697564611867647, 1.0), (1, 0.4774887508467626, 0.8), (2, 0.0, 0.3)]}
reg_proxintinf = {'type': 'region', 'value': [(0, 0.0, 1.0), (1, 0.0, 0.8), (2, 0.0, 0.3)]}
reg_distint = {'type': 'region', 'value': [(1, 0.5, 0.8225112491532374), (2, 0.7, 1.0), (3, 0.0, 0.432615327328525), (4, 0.0, 0.3628424955125098), (5, 0.1, 0.6)]}
reg_distintinf = {'type': 'region', 'value': [(1, 0.5, 1.0), (2, 0.7, 1.0), (3, 0.0, 1.0), (4, 0.0, 1.0), (5, 0.1, 1.0)]}
reg_lhs = {'type': 'region', 'value': [(0, 0.5, 1.0), (1, 0.0, 0.5)]}
reg_rhs = {'type': 'region', 'value': [(1, 0.0, 1.0)]}
reg_and = {'type': 'region', 'value': [(1, 0.0, 0.5)]}
reg_or = {'type': 'region', 'value': [(0, 0.5, 1.0), (1, 0.0, 1.0)]}
reg_diff = {'type': 'region', 'value': [(0, 0.5, 1.0)]}
reg_complement = {'type': 'region', 'value': [(0, 0.0, 1.0), (2, 0.0, 1.0), (3, 0.0, 1.0), (4, 0.0, 1.0), (5, 0.0, 1.0)]}
reg_completion = {'type': 'region', 'value': [(0, 1.0, 1.0), (1, 0.0, 1.0), (2, 0.0, 0.0)]}

############# locsets (tutorial_morph)

tut_ls_root  = {'type': 'locset', 'value': [(0, 0.0)]}
tut_ls_terminal  = {'type': 'locset', 'value': [(1, 1.0), (3, 1.0), (4, 1.0), (5, 1.0)]}
tut_ls_custom_terminal  = {'type': 'locset', 'value': [(3, 1.0), (4, 1.0)]}
tut_ls_axon_terminal  = {'type': 'locset', 'value': [(5, 1.0)]}

############# regions (tutorial_morph)

tut_reg_all = {'type': 'region', 'value': [(0, 0.0, 1.0), (1, 0.0, 1.0), (2, 0.0, 1.0), (3, 0.0, 1.0), (4, 0.0, 1.0), (5, 0.0, 1.0)]}
tut_reg_soma = {'type': 'region', 'value': [(0, 0.0, 0.3324708796524168)]}
tut_reg_axon = {'type': 'region', 'value': [(5, 0.0, 1.0)]}
tut_reg_dend = {'type': 'region', 'value': [(0, 0.3324708796524168, 1.0), (1, 0.0, 1.0), (2, 0.0, 1.0)]}
tut_reg_last = {'type': 'region', 'value': [(3, 0.0, 1.0), (4, 0.0, 1.0)]}
tut_reg_rad_gt = {'type': 'region', 'value': [(0, 0.0, 0.3324708796524168), (5, 0.0, 0.21875)]}
tut_reg_custom = {'type': 'region', 'value': [(0, 0.0, 0.3324708796524168), (3, 0.0, 1.0), (4, 0.0, 1.0), (5, 0.0, 0.21875)]}

############# locsets (tutorial_network_ring_morph)

tut_network_ring_ls_synapse_site  = {'type': 'locset', 'value': [(1, 0.5)]}
tut_network_ring_ls_root  = {'type': 'locset', 'value': [(0, 0.0)]}

############# regions (tutorial_network_ring_morph)

tut_network_ring_reg_soma = {'type': 'region', 'value': [(0, 0.0, 0.1935483870967742)]}
tut_network_ring_reg_dend = {'type': 'region', 'value': [(0, 0.1935483870967742, 1.0), (1, 0.0, 1.0), (2, 0.0, 1.0)]}
