#include "VisionHandMenuWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Styling/CoreStyle.h"
#include "VisionConditionLibrary.h"

#define LOCTEXT_NAMESPACE "VisionHandMenu"

namespace VisionMenuStyle
{
	// Colors are given in sRGB hex (as in a color picker) and converted to linear.
	// All fills are opaque: the ISDK widget material is masked, so partial transparency would be cut away.
	FLinearColor Hex(const TCHAR* Value)
	{
		return FLinearColor(FColor::FromHex(Value));
	}

	const FLinearColor Panel = Hex(TEXT("14171D"));
	const FLinearColor PanelOutline = Hex(TEXT("2E3542"));
	const FLinearColor Tile = Hex(TEXT("262C37"));
	const FLinearColor TileHovered = Hex(TEXT("364050"));
	const FLinearColor Accent = Hex(TEXT("4C8DFF"));
	const FLinearColor AccentHovered = Hex(TEXT("7AABFF"));
	const FLinearColor Danger = Hex(TEXT("3A2328"));
	const FLinearColor DangerHovered = Hex(TEXT("5A2E35"));
	const FLinearColor DangerPressed = Hex(TEXT("E5484D"));
	const FLinearColor TextPrimary = Hex(TEXT("F2F4F8"));
	const FLinearColor TextSecondary = Hex(TEXT("A9B1C1"));

	constexpr float PanelRadius = 28.0f;
	constexpr float ButtonRadius = 14.0f;
	constexpr float PanelPadding = 20.0f;

	// Touch targets: 1 px of the menu is ~0.35 mm in the world, so 68 px ~ 2.4 cm.
	constexpr float TileHeight = 68.0f;
	constexpr float ButtonHeight = 64.0f;
	constexpr float StepButtonSize = 72.0f;

	FButtonStyle MakeButtonStyle(const FLinearColor& Normal, const FLinearColor& Hovered, const FLinearColor& Pressed)
	{
		FButtonStyle Style;
		Style.SetNormal(FSlateRoundedBoxBrush(Normal, ButtonRadius));
		Style.SetHovered(FSlateRoundedBoxBrush(Hovered, ButtonRadius));
		Style.SetPressed(FSlateRoundedBoxBrush(Pressed, ButtonRadius));
		Style.SetDisabled(FSlateRoundedBoxBrush(Normal, ButtonRadius));
		Style.SetNormalPadding(FMargin(10.0f, 6.0f));
		Style.SetPressedPadding(FMargin(10.0f, 7.0f, 10.0f, 5.0f));
		return Style;
	}

	FSliderStyle MakeSliderStyle()
	{
		const FSlateRoundedBoxBrush Bar(PanelOutline, 5.0f);
		const FSlateRoundedBoxBrush Thumb(Accent, 20.0f, FVector2f(40.0f, 40.0f));
		const FSlateRoundedBoxBrush ThumbHovered(AccentHovered, 20.0f, FVector2f(40.0f, 40.0f));

		FSliderStyle Style;
		Style.SetNormalBarImage(Bar);
		Style.SetHoveredBarImage(Bar);
		Style.SetDisabledBarImage(Bar);
		Style.SetNormalThumbImage(Thumb);
		Style.SetHoveredThumbImage(ThumbHovered);
		Style.SetDisabledThumbImage(Thumb);
		Style.SetBarThickness(10.0f);
		return Style;
	}
}

using namespace VisionMenuStyle;

// ---------------------------------------------------------------------------------------------
// UVisionConditionButton
// ---------------------------------------------------------------------------------------------

void UVisionConditionButton::Setup(UVisionMenuWidget* InMenu, EVisionCondition InCondition)
{
	Menu = InMenu;
	Condition = InCondition;
	OnClicked.AddUniqueDynamic(this, &UVisionConditionButton::HandleClicked);
}

void UVisionConditionButton::HandleClicked()
{
	if (UVisionMenuWidget* MenuWidget = Menu.Get())
	{
		MenuWidget->SelectCondition(Condition);
	}
}

// ---------------------------------------------------------------------------------------------
// UVisionHandMenuWidget
// ---------------------------------------------------------------------------------------------

bool UVisionHandMenuWidget::Initialize()
{
	const bool bResult = Super::Initialize();
	// A native widget gets an empty widget tree; a Blueprint subclass with its own layout keeps it.
	if (bResult && WidgetTree && !WidgetTree->RootWidget)
	{
		BuildLayout();
	}
	return bResult;
}

UTextBlock* UVisionHandMenuWidget::MakeText(const FText& Text, int32 Size, bool bBold, const FLinearColor& Color)
{
	UTextBlock* TextBlock = WidgetTree->ConstructWidget<UTextBlock>();
	TextBlock->SetText(Text);
	TextBlock->SetFont(FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size));
	TextBlock->SetColorAndOpacity(FSlateColor(Color));
	TextBlock->SetAutoWrapText(true);
	return TextBlock;
}

UButton* UVisionHandMenuWidget::MakeButton(const FText& Label, float Height, bool bDanger, UTextBlock** OutLabel)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>();
	Button->SetStyle(bDanger
		? MakeButtonStyle(Danger, DangerHovered, DangerPressed)
		: MakeButtonStyle(Tile, TileHovered, Accent));

	UTextBlock* Text = MakeText(Label, 18, true, TextPrimary);
	Text->SetJustification(ETextJustify::Center);
	if (UButtonSlot* ContentSlot = Cast<UButtonSlot>(Button->SetContent(Text)))
	{
		ContentSlot->SetHorizontalAlignment(HAlign_Center);
		ContentSlot->SetVerticalAlignment(VAlign_Center);
	}
	if (OutLabel)
	{
		*OutLabel = Text;
	}

	if (Height <= 0.0f)
	{
		return Button;
	}
	// Fixed height for a comfortable touch target: wrap in a size box, return the button (for events).
	USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>();
	SizeBox->SetHeightOverride(Height);
	SizeBox->SetContent(Button);
	return Button;
}

void UVisionHandMenuWidget::BuildLayout()
{
	USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("MenuRoot"));
	Root->SetWidthOverride(MenuWidth);
	Root->SetHeightOverride(MenuHeight);
	WidgetTree->RootWidget = Root;

	UBorder* PanelBorder = WidgetTree->ConstructWidget<UBorder>();
	FSlateRoundedBoxBrush PanelBrush(Panel, PanelRadius);
	PanelBrush.OutlineSettings = FSlateBrushOutlineSettings(PanelRadius, FSlateColor(PanelOutline), 2.0f);
	PanelBorder->SetBrush(PanelBrush);
	PanelBorder->SetPadding(FMargin(PanelPadding));
	Root->SetContent(PanelBorder);

	UVerticalBox* Main = WidgetTree->ConstructWidget<UVerticalBox>();
	PanelBorder->SetContent(Main);

	// Header
	UTextBlock* Header = MakeText(LOCTEXT("Header", "VISION SIMULATOR"), 12, true, Accent);
	FSlateFontInfo HeaderFont = FCoreStyle::GetDefaultFontStyle("Bold", 12);
	HeaderFont.LetterSpacing = 250;
	Header->SetFont(HeaderFont);
	Main->AddChildToVerticalBox(Header);

	StatusText = MakeText(FText::GetEmpty(), 22, true, TextPrimary);
	Main->AddChildToVerticalBox(StatusText)->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 10.0f));

	// Separator line
	USizeBox* SeparatorBox = WidgetTree->ConstructWidget<USizeBox>();
	SeparatorBox->SetHeightOverride(2.0f);
	UBorder* Separator = WidgetTree->ConstructWidget<UBorder>();
	Separator->SetBrush(FSlateRoundedBoxBrush(PanelOutline, 1.0f));
	SeparatorBox->SetContent(Separator);
	Main->AddChildToVerticalBox(SeparatorBox)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));

	// Pages, in EVisionMenuPage order
	PageSwitcher = WidgetTree->ConstructWidget<UWidgetSwitcher>();
	PageSwitcher->AddChild(BuildConditionListPage());
	PageSwitcher->AddChild(BuildActiveConditionPage());
	PageSwitcher->AddChild(BuildAdjustConditionPage());
	PageSwitcher->AddChild(BuildConfirmExitPage());
	Main->AddChildToVerticalBox(PageSwitcher)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	// Footer: comparison mode (normal | impaired split screen) and exit
	UHorizontalBox* FooterRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	Footer = FooterRow;

	UTextBlock* CompareLabel = nullptr;
	UButton* CompareButton = MakeButton(FText::GetEmpty(), 56.0f, false, &CompareLabel);
	CompareText = CompareLabel;
	CompareButton->OnClicked.AddUniqueDynamic(this, &UVisionHandMenuWidget::HandleCompareClicked);
	UHorizontalBoxSlot* CompareSlot = FooterRow->AddChildToHorizontalBox(CompareButton->GetParent());
	CompareSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	CompareSlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));

	UButton* ExitButton = MakeButton(LOCTEXT("Exit", "Exit"), 56.0f, true);
	ExitButton->OnClicked.AddUniqueDynamic(this, &UVisionHandMenuWidget::HandleExitClicked);
	USizeBox* ExitBox = Cast<USizeBox>(ExitButton->GetParent());
	ExitBox->SetWidthOverride(130.0f);
	FooterRow->AddChildToHorizontalBox(ExitBox);

	Main->AddChildToVerticalBox(FooterRow)->SetPadding(FMargin(0.0f, 12.0f, 0.0f, 0.0f));
}

UWidget* UVisionHandMenuWidget::BuildConditionListPage()
{
	ConditionGrid = WidgetTree->ConstructWidget<UUniformGridPanel>();
	ConditionGrid->SetSlotPadding(FMargin(4.0f));
	ConditionGrid->SetMinDesiredSlotHeight(TileHeight);

	const TArray<EVisionCondition> Conditions = UVisionConditionLibrary::GetSelectableConditions();
	for (int32 Index = 0; Index < Conditions.Num(); ++Index)
	{
		UVisionConditionButton* TileButton = WidgetTree->ConstructWidget<UVisionConditionButton>();
		TileButton->SetStyle(MakeButtonStyle(VisionMenuStyle::Tile, TileHovered, Accent));
		TileButton->Setup(this, Conditions[Index]);

		UTextBlock* Label = MakeText(UVisionConditionLibrary::GetConditionName(Conditions[Index]), 16, true, TextPrimary);
		Label->SetJustification(ETextJustify::Center);
		if (UButtonSlot* ContentSlot = Cast<UButtonSlot>(TileButton->SetContent(Label)))
		{
			ContentSlot->SetHorizontalAlignment(HAlign_Center);
			ContentSlot->SetVerticalAlignment(VAlign_Center);
		}

		UUniformGridSlot* GridSlot = ConditionGrid->AddChildToUniformGrid(TileButton, Index / 2, Index % 2);
		GridSlot->SetHorizontalAlignment(HAlign_Fill);
		GridSlot->SetVerticalAlignment(VAlign_Fill);
	}
	return ConditionGrid;
}

UWidget* UVisionHandMenuWidget::BuildActiveConditionPage()
{
	UVerticalBox* Page = WidgetTree->ConstructWidget<UVerticalBox>();

	ActiveDescriptionText = MakeText(FText::GetEmpty(), 16, false, TextSecondary);
	Page->AddChildToVerticalBox(ActiveDescriptionText);

	Page->AddChildToVerticalBox(WidgetTree->ConstructWidget<USpacer>())->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	UButton* Back = MakeButton(LOCTEXT("Back", "Back"), ButtonHeight, true);
	Back->OnClicked.AddUniqueDynamic(this, &UVisionHandMenuWidget::HandleBackClicked);
	Page->AddChildToVerticalBox(Back->GetParent());
	return Page;
}

UWidget* UVisionHandMenuWidget::BuildAdjustConditionPage()
{
	UVerticalBox* Page = WidgetTree->ConstructWidget<UVerticalBox>();

	SliderLabelText = MakeText(FText::GetEmpty(), 14, false, TextSecondary);
	SliderLabelText->SetJustification(ETextJustify::Center);
	Page->AddChildToVerticalBox(SliderLabelText);

	SliderValueText = MakeText(FText::GetEmpty(), 40, true, TextPrimary);
	SliderValueText->SetJustification(ETextJustify::Center);
	Page->AddChildToVerticalBox(SliderValueText)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));

	// [-]  slider  [+]   The step buttons give exact 0.25 D steps; dragging in mid-air is less precise.
	UHorizontalBox* SliderRow = WidgetTree->ConstructWidget<UHorizontalBox>();

	UButton* Minus = MakeButton(FText::FromString(TEXT("-")), 0.0f, false);
	Minus->OnClicked.AddUniqueDynamic(this, &UVisionHandMenuWidget::HandleMinusClicked);
	USizeBox* MinusBox = WidgetTree->ConstructWidget<USizeBox>();
	MinusBox->SetWidthOverride(StepButtonSize);
	MinusBox->SetHeightOverride(StepButtonSize);
	MinusBox->SetContent(Minus);
	SliderRow->AddChildToHorizontalBox(MinusBox);

	ValueSlider = WidgetTree->ConstructWidget<USlider>();
	ValueSlider->SetWidgetStyle(MakeSliderStyle());
	ValueSlider->SetSliderBarColor(FLinearColor::White);
	ValueSlider->SetSliderHandleColor(FLinearColor::White);
	ValueSlider->MouseUsesStep = true;
	ValueSlider->RequiresControllerLock = false;
	ValueSlider->IsFocusable = false;
	ValueSlider->OnValueChanged.AddUniqueDynamic(this, &UVisionHandMenuWidget::HandleSliderChanged);
	UHorizontalBoxSlot* SliderSlot = SliderRow->AddChildToHorizontalBox(ValueSlider);
	SliderSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	SliderSlot->SetVerticalAlignment(VAlign_Center);
	SliderSlot->SetPadding(FMargin(14.0f, 0.0f));

	UButton* Plus = MakeButton(FText::FromString(TEXT("+")), 0.0f, false);
	Plus->OnClicked.AddUniqueDynamic(this, &UVisionHandMenuWidget::HandlePlusClicked);
	USizeBox* PlusBox = WidgetTree->ConstructWidget<USizeBox>();
	PlusBox->SetWidthOverride(StepButtonSize);
	PlusBox->SetHeightOverride(StepButtonSize);
	PlusBox->SetContent(Plus);
	SliderRow->AddChildToHorizontalBox(PlusBox);

	Page->AddChildToVerticalBox(SliderRow);

	SharpRangeText = MakeText(FText::GetEmpty(), 16, true, Accent);
	SharpRangeText->SetJustification(ETextJustify::Center);
	Page->AddChildToVerticalBox(SharpRangeText)->SetPadding(FMargin(0.0f, 14.0f, 0.0f, 0.0f));

	AdjustDescriptionText = MakeText(FText::GetEmpty(), 14, false, TextSecondary);
	Page->AddChildToVerticalBox(AdjustDescriptionText)->SetPadding(FMargin(0.0f, 12.0f, 0.0f, 0.0f));

	Page->AddChildToVerticalBox(WidgetTree->ConstructWidget<USpacer>())->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	UButton* Back = MakeButton(LOCTEXT("Back", "Back"), ButtonHeight, true);
	Back->OnClicked.AddUniqueDynamic(this, &UVisionHandMenuWidget::HandleBackClicked);
	Page->AddChildToVerticalBox(Back->GetParent());
	return Page;
}

UWidget* UVisionHandMenuWidget::BuildConfirmExitPage()
{
	UVerticalBox* Page = WidgetTree->ConstructWidget<UVerticalBox>();

	UTextBlock* Question = MakeText(LOCTEXT("ExitInfo", "The simulator will close. Unsaved settings are lost."), 16, false, TextSecondary);
	Page->AddChildToVerticalBox(Question);

	Page->AddChildToVerticalBox(WidgetTree->ConstructWidget<USpacer>())->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	// Cancel is the large, safe default; Exit is red. Kept apart so a single touch cannot hit both.
	UButton* Cancel = MakeButton(LOCTEXT("Cancel", "Cancel"), ButtonHeight, false);
	Cancel->OnClicked.AddUniqueDynamic(this, &UVisionHandMenuWidget::HandleExitCancelled);
	Page->AddChildToVerticalBox(Cancel->GetParent())->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 16.0f));

	UButton* Exit = MakeButton(LOCTEXT("ExitConfirm", "Exit"), ButtonHeight, true);
	Exit->OnClicked.AddUniqueDynamic(this, &UVisionHandMenuWidget::HandleExitConfirmed);
	Page->AddChildToVerticalBox(Exit->GetParent());
	return Page;
}

void UVisionHandMenuWidget::NativeOnMenuRefresh(EVisionMenuPage Page)
{
	if (!PageSwitcher)
	{
		return;
	}

	PageSwitcher->SetActiveWidgetIndex(static_cast<int32>(Page));

	switch (Page)
	{
	case EVisionMenuPage::ConditionList:
		StatusText->SetText(LOCTEXT("ChooseCondition", "Choose a condition"));
		break;
	case EVisionMenuPage::ConfirmExit:
		StatusText->SetText(LOCTEXT("ExitQuestion", "Exit the app?"));
		break;
	default:
		StatusText->SetText(GetConditionName());
		break;
	}
	Footer->SetVisibility(Page == EVisionMenuPage::ConfirmExit ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	ActiveDescriptionText->SetText(GetConditionDescription());
	AdjustDescriptionText->SetText(GetConditionDescription());

	// Range before value, otherwise the value gets clamped to the old range.
	float Min = 0.0f;
	float Max = 1.0f;
	float Step = 0.0f;
	GetSliderRange(Min, Max, Step);
	ValueSlider->SetMinValue(Min);
	ValueSlider->SetMaxValue(Max);
	ValueSlider->SetStepSize(Step > 0.0f ? Step : 0.01f);
	ValueSlider->SetValue(GetSliderValue()); // does not fire OnValueChanged

	SliderLabelText->SetText(GetSliderLabel());
	SliderValueText->SetText(GetSliderValueText());
	SharpRangeText->SetText(GetSharpRangeText());

	CompareText->SetText(IsCompareMode()
		? LOCTEXT("CompareOn", "Compare: ON")
		: LOCTEXT("CompareOff", "Compare: OFF"));
}

void UVisionHandMenuWidget::HandleBackClicked()
{
	Back();
}

void UVisionHandMenuWidget::HandleMinusClicked()
{
	StepSlider(-1.0f);
}

void UVisionHandMenuWidget::HandlePlusClicked()
{
	StepSlider(1.0f);
}

void UVisionHandMenuWidget::StepSlider(float Direction)
{
	float Min = 0.0f;
	float Max = 0.0f;
	float Step = 0.0f;
	GetSliderRange(Min, Max, Step);
	SetSliderValue(GetSliderValue() + Direction * Step);
}

void UVisionHandMenuWidget::HandleCompareClicked()
{
	ToggleCompareMode();
}

void UVisionHandMenuWidget::HandleExitClicked()
{
	RequestExit();
}

void UVisionHandMenuWidget::HandleExitConfirmed()
{
	ConfirmExit();
}

void UVisionHandMenuWidget::HandleExitCancelled()
{
	CancelExit();
}

void UVisionHandMenuWidget::HandleSliderChanged(float Value)
{
	SetSliderValue(Value);
}

#undef LOCTEXT_NAMESPACE
