#include "VisionConditionLibrary.h"

#define LOCTEXT_NAMESPACE "VisionConditions"

TArray<EVisionCondition> UVisionConditionLibrary::GetSelectableConditions()
{
	return {
		EVisionCondition::Myopia,
		EVisionCondition::Hyperopia,
		EVisionCondition::Presbyopia,
		EVisionCondition::Cataract,
		EVisionCondition::Glaucoma,
		EVisionCondition::MacularDegeneration,
		EVisionCondition::DiabeticRetinopathy,
		EVisionCondition::Protanopia,
		EVisionCondition::Deuteranopia,
		EVisionCondition::Tritanopia,
		EVisionCondition::Achromatopsia
	};
}

FText UVisionConditionLibrary::GetConditionName(EVisionCondition Condition)
{
	switch (Condition)
	{
	case EVisionCondition::None:                return LOCTEXT("None", "Normal vision");
	case EVisionCondition::Protanopia:          return LOCTEXT("Protanopia", "Protanopia");
	case EVisionCondition::Deuteranopia:        return LOCTEXT("Deuteranopia", "Deuteranopia");
	case EVisionCondition::Tritanopia:          return LOCTEXT("Tritanopia", "Tritanopia");
	case EVisionCondition::Achromatopsia:       return LOCTEXT("Achromatopsia", "Achromatopsia");
	case EVisionCondition::Cataract:            return LOCTEXT("Cataract", "Cataract");
	case EVisionCondition::Glaucoma:            return LOCTEXT("Glaucoma", "Glaucoma");
	case EVisionCondition::MacularDegeneration: return LOCTEXT("MacularDegeneration", "Macular degeneration (AMD)");
	case EVisionCondition::Myopia:              return LOCTEXT("Myopia", "Myopia (nearsightedness)");
	case EVisionCondition::Hyperopia:           return LOCTEXT("Hyperopia", "Hyperopia (farsightedness)");
	case EVisionCondition::Presbyopia:          return LOCTEXT("Presbyopia", "Presbyopia");
	case EVisionCondition::DiabeticRetinopathy: return LOCTEXT("DiabeticRetinopathy", "Diabetic retinopathy");
	default:                                    return FText::GetEmpty();
	}
}

FText UVisionConditionLibrary::GetConditionDescription(EVisionCondition Condition)
{
	switch (Condition)
	{
	case EVisionCondition::None:
		return LOCTEXT("NoneDesc", "Unprocessed camera image.");
	case EVisionCondition::Protanopia:
		return LOCTEXT("ProtanopiaDesc", "Red-green color blindness: no working L (red) cones. Reds look dark and are confused with greens.");
	case EVisionCondition::Deuteranopia:
		return LOCTEXT("DeuteranopiaDesc", "Red-green color blindness: no working M (green) cones. The most common color vision deficiency.");
	case EVisionCondition::Tritanopia:
		return LOCTEXT("TritanopiaDesc", "Blue-yellow color blindness: no working S (blue) cones. Rare.");
	case EVisionCondition::Achromatopsia:
		return LOCTEXT("AchromatopsiaDesc", "Total color blindness: no working cones. Only shades of gray, low acuity and strong glare sensitivity.");
	case EVisionCondition::Cataract:
		return LOCTEXT("CataractDesc", "Clouding of the lens: blurred, hazy, yellowish vision with glare around light sources.");
	case EVisionCondition::Glaucoma:
		return LOCTEXT("GlaucomaDesc", "Optic nerve damage: the visual field narrows from the periphery (tunnel vision), often unnoticed for years.");
	case EVisionCondition::MacularDegeneration:
		return LOCTEXT("MacularDegenerationDesc", "Damage to the macula: a blind spot and distorted lines in the center, peripheral vision stays.");
	case EVisionCondition::Myopia:
		return LOCTEXT("MyopiaDesc", "The eye focuses light in front of the retina: distant objects are blurred, near ones stay sharp. Uses real depth from the headset.");
	case EVisionCondition::Hyperopia:
		return LOCTEXT("HyperopiaDesc", "The eye focuses light behind the retina. Accommodation compensates up to a point; near objects blur first. Uses real depth from the headset.");
	case EVisionCondition::Presbyopia:
		return LOCTEXT("PresbyopiaDesc", "With age the lens loses elasticity and cannot focus on near objects. Uses real depth from the headset.");
	case EVisionCondition::DiabeticRetinopathy:
		return LOCTEXT("DiabeticRetinopathyDesc", "Damaged retinal blood vessels: dark patches, floaters and blurred vision.");
	default:
		return FText::GetEmpty();
	}
}

EVisionConditionControl UVisionConditionLibrary::GetConditionControl(EVisionCondition Condition)
{
	switch (Condition)
	{
	case EVisionCondition::Myopia:
	case EVisionCondition::Hyperopia:
		return EVisionConditionControl::RefractiveError;
	case EVisionCondition::Presbyopia:
		return EVisionConditionControl::Age;
	default:
		return EVisionConditionControl::None;
	}
}

FText UVisionConditionLibrary::FormatDistance(float Meters)
{
	if (Meters < 0.0f)
	{
		return LOCTEXT("Infinity", "infinity");
	}
	if (Meters < 1.0f)
	{
		return FText::Format(LOCTEXT("Centimeters", "{0} cm"), FText::AsNumber(FMath::RoundToInt(Meters * 100.0f)));
	}
	FNumberFormattingOptions Options;
	Options.MinimumFractionalDigits = 1;
	Options.MaximumFractionalDigits = 1;
	return FText::Format(LOCTEXT("Meters", "{0} m"), FText::AsNumber(Meters, &Options));
}

#undef LOCTEXT_NAMESPACE
